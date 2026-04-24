#ifndef UTILS_INT8_H
#define UTILS_INT8_H

#include <stdint.h>

#include "ecg_model_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ecg_model_result_t
{
    int label; // 0/1
    int32_t score_q; // optional fixed-point score (Q15-ish), 0 if unused
} ecg_model_result_t;

// Z-score standardization in fixed-point.
// Input: raw features (same units as TestSetData.txt first 6 columns)
// Output: standardized features quantized to int8 (symmetric, per-feature scale).
void ecg_standardize_and_quantize_int8(const int32_t x_raw[FEAT_DIM], int8_t x_q[FEAT_DIM]);

// Parse a line containing 6 features + label.
// Supports tabs/spaces separators.
int ecg_parse_sample_line(const char *line, int32_t x_raw[FEAT_DIM], int *out_label);

#ifdef __cplusplus
}
#endif

#endif // UTILS_INT8_H

