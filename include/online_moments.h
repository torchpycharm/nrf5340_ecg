/* online_moments.h
 * Online (incremental) moments accumulator (Welford / Pébay extension)
 * Provides per-sample updates and functions to compute variance, skewness,
 * and excess kurtosis consistent with common sample-corrected formulas.
 *
 * Small footprint; use double for stability.
 */

#ifndef ONLINE_MOMENTS_H
#define ONLINE_MOMENTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t n;
    double mean;
    double M2;
    double M3;
    double M4;
} online_moments_t;

static inline void online_moments_init(online_moments_t *s)
{
    s->n = 0;
    s->mean = 0.0;
    s->M2 = 0.0;
    s->M3 = 0.0;
    s->M4 = 0.0;
}

void online_moments_update(online_moments_t *s, double x);

/* sample variance (unbiased, divided by n-1). Returns NAN when n<2 */
double online_moments_variance(const online_moments_t *s);

/* population skewness (MATLAB default): m3 / m2^(3/2). Returns NAN when n<3 */
double online_moments_skewness(const online_moments_t *s);

/* raw kurtosis (MATLAB default): m4 / m2^2 (NOT excess). Returns NAN when n<4 */
double online_moments_kurtosis_raw(const online_moments_t *s);

#ifdef __cplusplus
}
#endif

#endif /* ONLINE_MOMENTS_H */
