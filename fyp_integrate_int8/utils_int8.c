#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "model_int8_params.h"
#include "utils_int8.h"

static int8_t clamp_i8(int32_t v)
{
    if (v < -128)
        return (int8_t)-128;
    if (v > 127)
        return (int8_t)127;
    return (int8_t)v;
}

static int32_t mul_q16(int32_t a_q16, int32_t b_q16)
{
    int64_t p = (int64_t)a_q16 * (int64_t)b_q16;
    // round
    p += (int64_t)1 << 15;
    return (int32_t)(p >> 16);
}

void ecg_standardize_and_quantize_int8(const int32_t x_raw[FEAT_DIM], int8_t x_q[FEAT_DIM])
{
    for (int d = 0; d < FEAT_DIM; d++)
    {
        // x_std = (x - mean) / std
        int32_t x_minus_mean_q16 = (x_raw[d] - g_mean_q16[d]); // Q16.16
        int32_t x_std_q16 = mul_q16(x_minus_mean_q16, g_inv_std_q16[d]); // Q16.16

        // x_q = round(x_std / feat_scale) = round(x_std * inv_feat_scale)
        int32_t x_scaled_q16 = mul_q16(x_std_q16, g_inv_feat_scale_q16[d]); // Q16.16

        // round Q16.16 to integer
        int32_t qi = (x_scaled_q16 + (1 << 15)) >> 16;
        x_q[d] = clamp_i8(qi);
    }
}

static int parse_q16_token(const char *s, const char **out_end, int32_t *out_q16)
{
    if (!s || !out_end || !out_q16)
        return 0;

    const char *p = s;
    while (*p && isspace((unsigned char)*p))
        p++;

    int sign = 1;
    if (*p == '+')
    {
        p++;
    }
    else if (*p == '-')
    {
        sign = -1;
        p++;
    }

    int saw_digit = 0;
    int64_t int_part = 0;
    while (*p && isdigit((unsigned char)*p))
    {
        saw_digit = 1;
        int digit = *p - '0';
        if (int_part > (INT64_MAX - digit) / 10)
            return 0;
        int_part = int_part * 10 + digit;
        p++;
    }

    int64_t frac_part = 0;
    int64_t frac_scale = 1;
    if (*p == '.')
    {
        p++;
        while (*p && isdigit((unsigned char)*p))
        {
            saw_digit = 1;
            int digit = *p - '0';
            if (frac_scale < 1000000000LL)
            {
                frac_part = frac_part * 10 + digit;
                frac_scale *= 10;
            }
            p++;
        }
    }

    if (!saw_digit)
        return 0;

    int64_t q = (int_part << 16);
    if (frac_scale > 1)
    {
        int64_t frac_q16 = (frac_part * (1LL << 16) + (frac_scale / 2)) / frac_scale;
        q += frac_q16;
    }

    q *= sign;
    if (q < INT32_MIN || q > INT32_MAX)
        return 0;
    *out_q16 = (int32_t)q;
    *out_end = p;
    return 1;
}

int ecg_parse_sample_line(const char *line, int32_t x_raw[FEAT_DIM], int *out_label)
{
    if (!line || !x_raw || !out_label)
        return 0;

    const char *p = line;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p == '\0')
        return 0;

    for (int d = 0; d < FEAT_DIM; d++)
    {
        const char *end = NULL;
        int32_t v_q16 = 0;
        if (!parse_q16_token(p, &end, &v_q16))
            return 0;
        x_raw[d] = v_q16;
        p = end;
        while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == '\t'))
            p++;
    }

    const char *end = NULL;
    int32_t label_q16 = 0;
    if (!parse_q16_token(p, &end, &label_q16))
        return 0;
    // label expected integer; accept q16 values too
    *out_label = (int)((label_q16 + (1 << 15)) >> 16);
    return 1;
}
