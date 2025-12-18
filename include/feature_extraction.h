/* feature_extraction.h */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "online_moments.h"
typedef struct {
    /* ==== configuration (constant) ==== */
    uint32_t fs;
    uint32_t window_sec;
    uint32_t seg_len;// 每段长度

    /* ==== runtime state ==== */
    uint32_t drop_count;
    uint32_t decim_count;

    double mean_acc;
    double mean_all;
    uint32_t mean_count;
     /* 三段统计器 */
    online_moments_t seg1;
    online_moments_t seg2;
    online_moments_t seg3;

    uint8_t seg_idx;// 当前段 (0,1,2)
    uint32_t seg_pos;// 当前段内部已写入样本数

    bool ready;
} feature_extraction_t;

void feature_extraction_init(feature_extraction_t *e, uint32_t fs, uint32_t window_sec);
bool feature_extraction_push(feature_extraction_t *e, int16_t raw_sample);
void feature_extraction_get(double out[6], feature_extraction_t *e);
void feature_extraction_reset(feature_extraction_t *e);
