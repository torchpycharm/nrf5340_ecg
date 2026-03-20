/* feature_extraction.h */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "online_moments.h"
typedef struct {
    /* ==== configuration (constant) ==== */
    uint32_t fs;
    uint32_t window_sec;

    /* ==== runtime state ==== */
    /* 三段统计器 */
    online_moments_t seg1;
    online_moments_t seg2;
    online_moments_t seg3;

    /* 缓冲区存储9000样本 */
    int16_t buffer[9000];
    uint32_t buffer_pos;

    bool ready;  /* 全部30秒是否处理完毕 */
} feature_extraction_t;

void feature_extraction_init(feature_extraction_t *e, uint32_t fs, uint32_t window_sec);
bool feature_extraction_push(feature_extraction_t *e, int16_t raw_sample);
void feature_extraction_get(double out[6], feature_extraction_t *e);
void feature_extraction_reset(feature_extraction_t *e);
void process_window(feature_extraction_t *e);