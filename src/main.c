/**
 * @file main.c
 * @brief NRF5340 ECG应用主程序
 * 
 * 架构：
 * UART线程接收 → ECG缓冲队列 → 处理线程 → 特征提取 → 30s完整性判断 → 蓝牙发送
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <errno.h>

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
#define ECG_BUFFER_SIZE     50
#define BLE_QUEUE_SIZE      10
#define ENABLE_BLE_OUTPUT   0

/* 当前默认模型: 先接入 Fnn_gboosting float，其他模型保留接口 */
#define ACTIVE_MODEL        ECG_MODEL_FNN_GBOOST_FLOAT

/* ===== 全局对象 ===== */
static ecg_buffer_t ecg_buffer;
static window_manager_t window_mgr;
static feature_extraction_t feature_eng;
static ecg_model_runtime_t model_runtime;

/**
 * @brief 主程序初始化
 */
int main(void)
{
    printk("=== NRF5340 ECG Application Start ===\n");
    printk("\n\n========================================\n");
    printk("   NRF5340 ECG Async Processing Demo\n");
    printk("   Sam: %u Hz, Window: %u sec\n", ECG_FS, WINDOW_SEC);
    printk("========================================\n\n");

    printk("Initializing ECG buffer...");
    int ret = ecg_buffer_init(&ecg_buffer, ECG_BUFFER_SIZE);
    if (ret != 0) {
        printk("Failed to initialize ECG buffer: %d\n", ret);
        return ret;
    }

    printk("Initializing UART sample receive...");
    ret = uart_sample_rx_init_async(&ecg_buffer);
    if (ret != 0) {
        printk("Failed to initialize UART sample RX: %d\n", ret);
        return ret;
    }

    printk("Initializing window manager...\n");
    window_manager_init(&window_mgr, ECG_FS, WINDOW_SEC);

    printk("Initializing feature extraction engine...\n");
    feature_extraction_init(&feature_eng, ECG_FS, WINDOW_SEC);

    ret = ecg_model_runtime_init(&model_runtime, ACTIVE_MODEL);
    if (ret != 0) {
        printk("Failed to initialize model runtime: %d\n", ret);
        return ret;
    }

    printk("Model runtime initialized: %s\n",
           ecg_model_runtime_name(model_runtime.active_model));

#if ENABLE_BLE_OUTPUT
    printk("Initializing BLE output...\n");
    ret = ble_output_init(BLE_QUEUE_SIZE);
    if (ret != 0) {
        printk("Failed to initialize BLE output: %d\n", ret);
        return ret;
    }
#else
    printk("BLE output disabled (local inference print mode)\n");
#endif

    printk("All modules initialized successfully!\n");
    printk("Starting ECG processing loop...\n");

    ecg_sample_t sample;
    window_integrity_t integrity;


    while (1) {
        int ret = ecg_buffer_pop(&ecg_buffer, &sample);
        if (ret == -ENODATA) {
            k_sleep(K_USEC(100));
            continue;
        }

        bool feature_done = feature_extraction_push(&feature_eng, sample.ecg_data);
        bool window_done = window_manager_push(&window_mgr, sample.seq_num, &integrity);

        if (!feature_done || !window_done) {
            continue;
        }

        if (!feature_eng.ready) {
            printk("Window completed but feature extraction not ready!\n");
            printk("Feature state: buffer_pos=%u/9000\n", feature_eng.buffer_pos);
            feature_extraction_reset(&feature_eng);
            window_manager_reset(&window_mgr);
            continue;
        }

        printk("\n===============================================\n");
        printk("30s window completed with valid features!\n");
        printk("===============================================\n");

        double features[6];
        feature_extraction_get(features, &feature_eng);

        printk("Window integrity: %.0f%% (%u/%u samples)\n",
               (double)integrity.integrity_percent,
               integrity.received_samples,
               integrity.expected_samples);

        printk("Features - Kurtosis: [%.4f, %.4f, %.4f]\n",
               features[0], features[1], features[2]);

        printk("Features - Skewness: [%.4f, %.4f, %.4f]\n",
               features[3], features[4], features[5]);

        /* 先快照并重置窗口，避免推理/发送占用下一轮采样窗口 */
        window_integrity_t integrity_snapshot = integrity;
        feature_extraction_reset(&feature_eng);
        window_manager_reset(&window_mgr);

        ecg_model_result_t infer_out = {0};
        ret = ecg_model_runtime_infer(&model_runtime, features, &infer_out);
        if (ret == -ENOTSUP) {
            printk("Inference backend not ready: %s\n",
                   ecg_model_runtime_name(model_runtime.active_model));
        } else if (ret != 0) {
            printk("Inference failed: %d\n", ret);
        } else {
            printk("Inference result: model=%s, label=%d, score=%.5f\n",
                   ecg_model_runtime_name(model_runtime.active_model),
                   infer_out.label,
                   (double)infer_out.score);
        }

        if (ret == 0 && integrity_snapshot.is_valid && infer_out.label == 1) {
            printk("Window VALID + model VALID\n");
#if ENABLE_BLE_OUTPUT
            ecg_result_t result = {
                .timestamp_ms = k_uptime_get_32(),
                .kurtosis = {features[0], features[1], features[2]},
                .skewness = {features[3], features[4], features[5]},
                .integrity = integrity_snapshot,
                .model_output = infer_out.label,
                .model_confidence = infer_out.score,
                .status = integrity_snapshot.is_valid ? 0 : 2,
            };

            ret = ble_output_queue_result(&result);
            if (ret != 0) {
                printk("Failed to queue result to BLE: %d\n", ret);
            }
#endif
        } else {
            printk("Window not sent: integrity=%u%%, label=%d, infer_ret=%d\n",
                   integrity_snapshot.integrity_percent,
                   (ret == 0) ? infer_out.label : -1,
                   ret);
        }

        printk("Ready for next 30s window\n");
    }

    return 0;
}

