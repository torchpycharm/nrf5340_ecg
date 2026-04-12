/**
 * @file ecg_processing.c
 * @brief ECG处理流程实现
 */

#include "ecg_processing.h"
#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ecg_processing, LOG_LEVEL_INF);

/**
 * @brief 初始化窗口管理器
 */
void window_manager_init(window_manager_t *mgr, uint32_t fs, uint32_t window_sec)
{
    if (!mgr) return;

    memset(mgr, 0, sizeof(*mgr));

    mgr->fs = fs;
    mgr->window_sec = window_sec;
    mgr->expected_samples = fs * window_sec;
    mgr->window_start_time_ms = k_uptime_get_32();
    mgr->seq_last = 0;

    LOG_INF("Window manager initialized: fs=%u, window=%us, expected=%u samples",
           fs, window_sec, mgr->expected_samples);
}

int window_manager_set_expected_samples(window_manager_t *mgr, uint32_t expected_samples)
{
    if (!mgr || expected_samples == 0) {
        return -EINVAL;
    }

    mgr->expected_samples = expected_samples;
    mgr->window_sec = (mgr->fs > 0) ? (expected_samples / mgr->fs) : 0;
    return 0;
}

/**
 * @brief 处理一个样本，检查是否完成了30秒窗口
 */
bool window_manager_push(window_manager_t *mgr, uint32_t seq_num, 
                         window_integrity_t *integrity)
{
    if (!mgr) return false;

    /* 序列号检查 - 检测是否有丢帧 */
    if (mgr->sample_count > 0) {
        uint32_t expected_seq = mgr->seq_last + 1;
        int32_t seq_delta = (int32_t)(seq_num - expected_seq);
        
        if (seq_delta > 1) {
            /* 检测到丢帧 */
            mgr->seq_gaps += seq_delta - 1;
            LOG_WRN("Sequence gap detected: expected %u, got %u (gap=%d)",
                   expected_seq, seq_num, seq_delta - 1);
        } else if (seq_delta < 0) {
            /* 乱序帧 */
            LOG_WRN("Out-of-order frame: expected %u, got %u", 
                   expected_seq, seq_num);
        }
    }

    mgr->seq_last = seq_num;
    mgr->sample_count++;

    /* 检查是否完成了30秒窗口 */
    bool window_done = (mgr->expected_samples > 0) &&
                       (mgr->sample_count >= mgr->expected_samples);

    if (window_done && integrity) {
        uint32_t window_end_time = k_uptime_get_32();
        uint32_t window_duration = window_end_time - mgr->window_start_time_ms;

        /* 填充完整性信息 */
        integrity->expected_samples = mgr->expected_samples;
        integrity->received_samples = mgr->sample_count;
        integrity->missing_samples = (mgr->sample_count >= mgr->expected_samples) ?
                         0 :
                         (mgr->expected_samples - mgr->sample_count);
        integrity->seq_gaps = mgr->seq_gaps;
        integrity->window_start_time_ms = mgr->window_start_time_ms;
        integrity->window_end_time_ms = window_end_time;
        integrity->window_duration_ms = window_duration;

        /* 计算完整性百分比 */
        if (mgr->expected_samples > 0) {
            integrity->integrity_percent = 
                (mgr->sample_count * 100) / mgr->expected_samples;
        } else {
            integrity->integrity_percent = 0;
        }

        /* 判断有效性 (>=95%) */
        integrity->is_valid = (integrity->integrity_percent >= 95);

        /* 统计 */
        mgr->windows_completed++;
        if (!integrity->is_valid) {
            mgr->windows_invalid++;
            LOG_WRN("Window completed but INVALID: %.0f%% complete, %u gaps",
                   (double)integrity->integrity_percent, mgr->seq_gaps);
        } else {
            LOG_INF("Window completed SUCCESSFULLY: %.0f%% complete",
                   (double)integrity->integrity_percent);
        }

        return true;
    }

    return false;
}

/**
 * @brief 重置窗口
 */
void window_manager_reset(window_manager_t *mgr)
{
    if (!mgr) return;

    mgr->sample_count = 0;
    mgr->seq_gaps = 0;
    mgr->seq_last = 0;
    mgr->window_start_time_ms = k_uptime_get_32();

    LOG_DBG("Window manager reset for next cycle");
}

/**
 * @brief 获取当前窗口的完整性信息（不重置）
 */
void window_manager_get_integrity(const window_manager_t *mgr, 
                                   window_integrity_t *integrity)
{
    if (!mgr || !integrity) return;

    uint32_t current_time = k_uptime_get_32();

    integrity->expected_samples = mgr->expected_samples;
    integrity->received_samples = mgr->sample_count;
    integrity->missing_samples = (mgr->sample_count >= mgr->expected_samples) ?
                                 0 :
                                 (mgr->expected_samples - mgr->sample_count);
    integrity->seq_gaps = mgr->seq_gaps;
    integrity->window_start_time_ms = mgr->window_start_time_ms;
    integrity->window_end_time_ms = current_time;
    integrity->window_duration_ms = current_time - mgr->window_start_time_ms;

    if (mgr->expected_samples > 0) {
        integrity->integrity_percent = 
            (mgr->sample_count * 100) / mgr->expected_samples;
    } else {
        integrity->integrity_percent = 0;
    }

    integrity->is_valid = (integrity->integrity_percent >= 95);
}

/**
 * @brief 获取窗口管理器统计
 */
void window_manager_get_stats(const window_manager_t *mgr, window_stats_t *stats)
{
    if (!mgr || !stats) return;

    memset(stats, 0, sizeof(*stats));

    stats->windows_completed = mgr->windows_completed;
    stats->windows_invalid = mgr->windows_invalid;

    if (mgr->windows_completed > 0) {
        stats->validity_percent = 
            ((mgr->windows_completed - mgr->windows_invalid) * 100.0f) / 
            mgr->windows_completed;
    } else {
        stats->validity_percent = 0.0f;
    }
}
