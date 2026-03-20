#include "feature_window.h"
#include <stdlib.h>
#include <string.h>

int feature_window_init(feature_window_t *w,
                         uint32_t fs,
                         uint32_t win_sec)
{
    memset(w, 0, sizeof(*w));
    w->fs = fs;
    w->win_sec = win_sec;

    uint32_t total = fs * win_sec;
    w->raw = malloc(total * sizeof(int16_t));
    if (!w->raw) return -1;

    return 0;
}

bool feature_window_push(feature_window_t *w, int16_t sample)
{
    if (w->ready) return true;

    w->raw[w->count++] = sample;

    if (w->count >= w->fs * w->win_sec) {
        w->ready = true;
        return true;
    }
    return false;
}

void feature_window_reset(feature_window_t *w)
{
    w->count = 0;
    w->ready = false;
}

/* MATLAB 等价：
 * mean → drop 1s → decimate /4 → subtract mean
 */
int feature_window_preprocess(feature_window_t *w,
                              double **out,
                              uint32_t *out_len)
{
    uint32_t N = w->count;
    if (N < w->fs) return -1;

    double mean = 0.0;
    for (uint32_t i = 0; i < N; i++)
        mean += w->raw[i];
    mean /= (double)N;

    uint32_t remain = N - w->fs;
    uint32_t cap = (remain + 3) / 4;

    double *rs = malloc(cap * sizeof(double));
    if (!rs) return -2;

    uint32_t idx = 0;
    for (uint32_t i = w->fs; i < N; i += 4)
        rs[idx++] = (double)w->raw[i] - mean;

    *out = rs;
    *out_len = idx;
    return 0;
}
