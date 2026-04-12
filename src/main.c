/**
 * @file main.c
 * @brief NRF5340 ECG应用主程序
 * 
 * 架构（异步快照版）：
 * UART ISR → ECG缓冲队列 → 主循环收集样本 → 快照 → k_work工作线程处理
 *   主循环：只负责收集，不做耗时计算，收满即快照+提交work+立即继续
 *   工作线程：process_window + 特征提取 + 模型推理 + (未来)BLE发送
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <string.h>

#include "uart_sample_rx.h"
#include "ecg_buffer.h"
#include "ecg_processing.h"
#include "feature_extraction.h"
#include "ble_output.h"
#include "models/model_runtime.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* ===== 配置常量 ===== */
#define ECG_FS              300
#define WINDOW_SEC          30
#define ECG_BUFFER_SIZE     200   /* 扩大缓冲区，覆盖快照memcpy期间ISR写入 */
#define BLE_QUEUE_SIZE      10
#define ENABLE_BLE_OUTPUT   1

/* 当前默认模型 */
#define ACTIVE_MODEL        ECG_MODEL_FNN_GBOOST_FLOAT

/* ===== 主循环收集用对象 ===== */
static ecg_buffer_t ecg_buffer;
static window_manager_t window_mgr;
static feature_extraction_t feature_eng;
static ecg_model_runtime_t model_runtime;

/* ===== 快照区：主循环写入，工作线程读取，信号量保护 ===== */
static feature_extraction_t snap_feature_eng;
static window_integrity_t   snap_integrity;
static uint16_t             snap_file_id;
static K_SEM_DEFINE(snap_sem, 1, 1);  /* 1=快照区空闲, 0=工作线程占用 */

/* ===== 工作队列 ===== */
static struct k_work process_work;
static uint32_t snap_timestamp_ms;     /* 快照时刻 */
static uint32_t windows_collected;     /* 主循环已收集窗口计数 */
static uint32_t windows_processed;     /* 工作线程已处理窗口计数 */
static uint32_t windows_skipped;       /* 快照区忙导致跳过的窗口计数 */

/**
 * @brief 工作线程处理函数
 * 
 * 在系统工作队列线程中执行，不阻塞主循环。
 * 流程：process_window → 特征提取 → 模型推理 → 打印结果 → 释放快照区  
 */
static void process_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    uint32_t work_t0 = k_uptime_get_32();

    printk("\n[WORK] === Processing snapshot file=%u (collected=%u) ===\n",
           snap_file_id, windows_collected);

    /* 1) 执行 process_window（耗时 ~154ms） */
    uint32_t pw_t0 = k_uptime_get_32();
    process_window(&snap_feature_eng);
    uint32_t pw_t1 = k_uptime_get_32();
    snap_feature_eng.last_extract_time_ms = (pw_t1 - pw_t0);

    if (!snap_feature_eng.ready) {
        printk("[WORK] process_window failed for file=%u\n", snap_file_id);
        windows_processed++;
        k_sem_give(&snap_sem);
        return;
    }

    /* 2) 提取特征 */
    double features[6];
    feature_extraction_get(features, &snap_feature_eng);

    printk("[WORK] Feature extraction time: %u ms\n", snap_feature_eng.last_extract_time_ms);
    printk("[WORK] Window integrity: %u%% (%u/%u samples)\n",
           snap_integrity.integrity_percent,
           snap_integrity.received_samples,
           snap_integrity.expected_samples);
    printk("[WORK] Kurtosis: [%.4f, %.4f, %.4f]\n",
           features[0], features[1], features[2]);
    printk("[WORK] Skewness: [%.4f, %.4f, %.4f]\n",
           features[3], features[4], features[5]);

    /* 3) 模型推理 */
    ecg_model_result_t infer_out = {0};
    uint32_t inf_t0 = k_uptime_get_32();
    int ret = ecg_model_runtime_infer(&model_runtime, features, &infer_out);
    uint32_t inf_t1 = k_uptime_get_32();

    printk("[WORK] Inference time: %u ms\n", (inf_t1 - inf_t0));

    if (ret == 0) {
        printk("[WORK] Inference: model=%s label=%d score=%.5f\n",
               ecg_model_runtime_name(model_runtime.active_model),
               infer_out.label, (double)infer_out.score);
    } else {
        printk("[WORK] Inference failed: %d\n", ret);
    }

    /* 4) 结果判断 + 备份数据可用性验证 */
    if (ret == 0 && snap_integrity.is_valid && infer_out.label == 1) {
        printk("[WORK] >>> VALID: file=%u label=1, backup %u samples ready for BLE <<<\n",
               snap_file_id, snap_feature_eng.buffer_pos);
        /* TODO: 蓝牙发送 snap_feature_eng.buffer[] 数据 */
    } else {
        printk("[WORK] Window not sent: integrity=%u%% label=%d ret=%d\n",
               snap_integrity.integrity_percent,
               (ret == 0) ? infer_out.label : -1, ret);
    }

    uint32_t work_t1 = k_uptime_get_32();
    printk("[WORK] Total work time: %u ms (snap_delay=%u ms)\n",
           (work_t1 - work_t0), (work_t0 - snap_timestamp_ms));

    windows_processed++;
    printk("[WORK] Stats: collected=%u processed=%u skipped=%u\n",
           windows_collected, windows_processed, windows_skipped);

    /* 5) 释放快照区，主循环可以写入下一个快照 */
    k_sem_give(&snap_sem);
}

/**
 * @brief 主程序
 */
int main(void)
{
    printk("=== NRF5340 ECG Application Start ===\n");
    printk("\n\n========================================\n");
    printk("   NRF5340 ECG Async Snapshot Demo\n");
    printk("   Sam: %u Hz, Window: %u sec\n", ECG_FS, WINDOW_SEC);
    printk("   ECG Buffer: %u slots\n", ECG_BUFFER_SIZE);
    printk("========================================\n\n");

    int ret = ecg_buffer_init(&ecg_buffer, ECG_BUFFER_SIZE);
    if (ret != 0) {
        printk("Failed to init ECG buffer: %d\n", ret);
        return ret;
    }

    ret = uart_sample_rx_init_async(&ecg_buffer);
    if (ret != 0) {
        printk("Failed to init UART RX: %d\n", ret);
        return ret;
    }

    window_manager_init(&window_mgr, ECG_FS, WINDOW_SEC);
    feature_extraction_init(&feature_eng, ECG_FS, WINDOW_SEC);

    ret = ecg_model_runtime_init(&model_runtime, ACTIVE_MODEL);
    if (ret != 0) {
        printk("Failed to init model runtime: %d\n", ret);
        return ret;
    }
    printk("Model: %s\n", ecg_model_runtime_name(model_runtime.active_model));

#if ENABLE_BLE_OUTPUT
    ret = ble_output_init(BLE_QUEUE_SIZE);
    if (ret != 0) {
        printk("Failed to init BLE output: %d\n", ret);
        return ret;
    }
#endif

    /* 初始化工作项 */
    k_work_init(&process_work, process_work_handler);
    windows_collected = 0;
    windows_processed = 0;
    windows_skipped = 0;

    printk("All modules initialized. Starting collection loop...\n");

    ecg_sample_t sample;
    window_integrity_t integrity;
    bool window_active = false;
    uint16_t current_file_id = 0;

    while (1) {
        /* ---- 检查新头帧（仅在非收集状态下接受） ---- */
        uart_window_header_t header;
        if (uart_sample_rx_try_get_window_header(&header)) {
            if (window_active) {
                /* 正在收集中，忽略此头帧（ISR数据继续入buffer会被下轮消费掉） */
                printk("[MAIN] Header for file=%u IGNORED (busy collecting file=%u, pos=%u/%u)\n",
                       header.file_id, current_file_id,
                       feature_eng.buffer_pos, feature_eng.target_samples);
            } else {
                ret = feature_extraction_set_target_samples(&feature_eng, header.sample_count);
                if (ret != 0) {
                    printk("[MAIN] Invalid sample count from header: %u\n", header.sample_count);
                    continue;
                }

                ret = window_manager_set_expected_samples(&window_mgr, header.sample_count);
                if (ret != 0) {
                    printk("[MAIN] Invalid window sample count: %u\n", header.sample_count);
                    continue;
                }

                feature_extraction_reset(&feature_eng);
                window_manager_reset(&window_mgr);

                current_file_id = header.file_id;
                window_active = true;

                printk("\n[MAIN] >>> Header file=%u samples=%u <<<\n",
                       header.file_id, header.sample_count);
            }
        }

        if (!window_active) {
            k_sleep(K_USEC(100));
            continue;
        }

        /* ---- 逐样本弹出并推入特征收集器 ---- */
        int pop_ret = ecg_buffer_pop(&ecg_buffer, &sample);
        if (pop_ret == -ENODATA) {
            k_sleep(K_USEC(100));
            continue;
        }

        bool feature_done = feature_extraction_push(&feature_eng, sample.ecg_data);
        bool window_done = window_manager_push(&window_mgr, sample.seq_num, &integrity);

        if (!feature_done || !window_done) {
            continue;
        }

        /* ---- 窗口收集完毕：快照 + 提交工作 ---- */
        uint32_t snap_t0 = k_uptime_get_32();
        windows_collected++;

        /* 尝试获取快照区锁（非阻塞） */
        if (k_sem_take(&snap_sem, K_NO_WAIT) != 0) {
            /* 上一个快照还在处理中，跳过本窗口 */
            windows_skipped++;
            printk("[MAIN] !!! SKIP file=%u: work thread busy (skipped=%u) !!!\n",
                   current_file_id, windows_skipped);
            feature_extraction_reset(&feature_eng);
            window_manager_reset(&window_mgr);
            window_active = false;
            continue;
        }

        /* 快照：memcpy feature_eng → snap_feature_eng（~18KB for 9000 samples） */
        memcpy(&snap_feature_eng, &feature_eng, sizeof(feature_extraction_t));
        snap_integrity = integrity;
        snap_file_id = current_file_id;
        snap_timestamp_ms = snap_t0;

        uint32_t snap_t1 = k_uptime_get_32();
        printk("[MAIN] Snapshot file=%u done in %u ms (buffer_pos=%u)\n",
               current_file_id, (snap_t1 - snap_t0), feature_eng.buffer_pos);

        /* 立即重置收集器，准备下一窗口 */
        feature_extraction_reset(&feature_eng);
        window_manager_reset(&window_mgr);
        window_active = false;

        /* 提交到系统工作队列（不阻塞主循环） */
        k_work_submit(&process_work);

        printk("[MAIN] Work submitted, resuming collection...\n");
    }

    return 0;
}

