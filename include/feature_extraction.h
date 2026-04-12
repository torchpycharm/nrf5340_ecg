/* feature_extraction.h */

/* feature_extraction.h */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "online_moments.h"

#define FEATURE_EXTRACTION_MAX_SAMPLES 12000

typedef struct {
    /* ==== configuration (constant) ==== */
    uint32_t fs;
    uint32_t window_sec;
    uint32_t target_samples;

    /* ==== runtime state ==== */
    /* 三段统计器 */
    online_moments_t seg1;
    online_moments_t seg2;
    online_moments_t seg3;

    /* 缓冲区存储12000样本 */
    int16_t buffer[FEATURE_EXTRACTION_MAX_SAMPLES];
    uint32_t buffer_pos;
    uint32_t last_extract_time_ms;

    bool ready;  /* 全部30秒是否处理完毕 */
} feature_extraction_t;

void feature_extraction_init(feature_extraction_t *e, uint32_t fs, uint32_t window_sec);
int feature_extraction_set_target_samples(feature_extraction_t *e, uint32_t target_samples);
bool feature_extraction_push(feature_extraction_t *e, int16_t raw_sample);
void feature_extraction_get(double out[6], feature_extraction_t *e);
void feature_extraction_reset(feature_extraction_t *e);
void process_window(feature_extraction_t *e);