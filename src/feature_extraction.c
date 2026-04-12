/* feature_extraction.c */

#include "feature_extraction.h"
#include <math.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>



void feature_extraction_init(feature_extraction_t *e,
                             uint32_t fs,
                             uint32_t window_sec)
{
    memset(e, 0, sizeof(*e));

    /* 保存配置 */
    e->fs = fs;
    e->window_sec = window_sec;
    e->target_samples = fs * window_sec;
    if (e->target_samples > FEATURE_EXTRACTION_MAX_SAMPLES) {
        e->target_samples = FEATURE_EXTRACTION_MAX_SAMPLES;
    }

    online_moments_init(&e->seg1);
    online_moments_init(&e->seg2);
    online_moments_init(&e->seg3);

    /* 初始化缓冲区 */
    e->buffer_pos = 0;
    e->last_extract_time_ms = 0;
    e->ready = false;
}

int feature_extraction_set_target_samples(feature_extraction_t *e, uint32_t target_samples)
{
    if (e == NULL || target_samples == 0 ||
        target_samples > FEATURE_EXTRACTION_MAX_SAMPLES) {
        return -1;
    }

    e->target_samples = target_samples;
    return 0;
}



/**
 * @brief 处理完整窗口数据（长度由 target_samples 决定）
 * 严格遵循MATLAB逻辑
 */
void process_window(feature_extraction_t *e)
{
    uint32_t N = e->target_samples;

    if (N == 0 || N > FEATURE_EXTRACTION_MAX_SAMPLES || N <= e->fs) {
        e->ready = false;
        printk("Invalid window length for feature extraction: %u\n", N);
        return;
    }

    printk("Processing window with %u samples...\n", N);

    /* 步骤1: 去均值 */
    double sum = 0.0;
    for (uint32_t i = 0; i < N; i++) {
        sum += e->buffer[i];
    }
    double mean = sum / (double)N;
    for (uint32_t i = 0; i < N; i++) {
        e->buffer[i] = (int16_t)((double)e->buffer[i] - mean);
    }

    /* 步骤2~4: 跳过前1秒后按1/4降采样，直接映射到三段统计，避免大栈数组 */
    uint32_t decimated_len = (N - e->fs) / 4;
    uint32_t seg_size = decimated_len / 3;

    if (seg_size == 0) {
        e->ready = false;
        printk("Window too short after decimation: N=%u\n", N);
        return;
    }

    for (uint32_t i = 0; i < seg_size; i++) {
        uint32_t idx = e->fs + i * 4;
        online_moments_update(&e->seg1, (double)e->buffer[idx]);
    }

    for (uint32_t i = 0; i < seg_size; i++) {
        uint32_t idx = e->fs + (seg_size + i) * 4;
        online_moments_update(&e->seg2, (double)e->buffer[idx]);
    }

    for (uint32_t i = 0; i < seg_size; i++) {
        uint32_t idx = e->fs + (2 * seg_size + i) * 4;
        online_moments_update(&e->seg3, (double)e->buffer[idx]);
    }

    e->ready = true;
}
/**
 * @brief 每来一个样本调用一次，处理流程：
 * 
 * 严格遵循MATLAB逻辑：
 * 1. 收集 target_samples 个样本到缓冲区
 * 2. 当收集完成时，进行预处理和特征计算
 * 3. 预处理：去均值 → 截取前1秒 → 降采样1/4
 * 4. 分段计算峰度和偏度
 * 
 * 返回值：整个30秒窗口处理完毕时返回 true
 */
bool feature_extraction_push(feature_extraction_t *e, int16_t raw_sample)
{
    /* 收集样本到缓冲区 */
    if (e->buffer_pos < e->target_samples) {
        e->buffer[e->buffer_pos] = raw_sample;
        e->buffer_pos++;
        
        /* 缓冲区满时仅返回 true，不在此处调用 process_window()
         * 调用者负责快照后在工作线程中执行 process_window */
        if (e->buffer_pos == e->target_samples) {
            return true;
        }
        return false;
    }

    /* 已处理完成，等待 reset */
    return false;
}
void feature_extraction_get(double out[6], feature_extraction_t *e)
{
    /* 前三维 kurtosis，后三维 skewness */
    out[0] = (double)online_moments_kurtosis_raw(&e->seg1);
    out[1] = (double)online_moments_kurtosis_raw(&e->seg2);
    out[2] = (double)online_moments_kurtosis_raw(&e->seg3);

    out[3] = (double)online_moments_skewness(&e->seg1);
    out[4] = (double)online_moments_skewness(&e->seg2);
    out[5] = (double)online_moments_skewness(&e->seg3);
}

void feature_extraction_reset(feature_extraction_t *e)
{
    /* 重置所有状态，准备下一个窗口 */
    online_moments_init(&e->seg1);
    online_moments_init(&e->seg2);
    online_moments_init(&e->seg3);

    e->buffer_pos = 0;
    e->last_extract_time_ms = 0;
    e->ready = false;
}
