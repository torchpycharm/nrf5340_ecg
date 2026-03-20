#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int16_t *raw;
    uint32_t fs;
    uint32_t win_sec;
    uint32_t count;
    bool     ready;
} feature_window_t;

int  feature_window_init(feature_window_t *w,
                         uint32_t fs,
                         uint32_t win_sec);
bool feature_window_push(feature_window_t *w, int16_t sample);
void feature_window_reset(feature_window_t *w);

/* 预处理输出：返回降采样后数组和长度 */
int feature_window_preprocess(feature_window_t *w,
                              double **out,
                              uint32_t *out_len);
