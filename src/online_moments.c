#include "online_moments.h"
#include <math.h>

void online_moments_update(online_moments_t *s, double x)
{
    uint32_t n1 = s->n;
    s->n = n1 + 1;
    double delta = x - s->mean;
    double delta_n = delta / (double)s->n;
    double delta_n2 = delta_n * delta_n;
    double term1 = delta * delta_n * (double)n1;

    double n = (double)s->n;

    s->mean += delta_n;

    s->M4 += term1 * delta_n2 * (n*n - 3.0*n + 3.0) + 6.0 * delta_n2 * s->M2 - 4.0 * delta_n * s->M3;
    s->M3 += term1 * delta_n * (n - 2.0) - 3.0 * delta_n * s->M2;
    s->M2 += term1;
}

double online_moments_variance(const online_moments_t *s)
{
    if (s->n < 2) return NAN;
    return s->M2 / (double)(s->n - 1);
}

double online_moments_skewness(const online_moments_t *s)
{
    if (s->n < 3) return NAN;
    double n = (double)s->n;
    /* MATLAB default (population) skewness: m3 / m2^(3/2) */
    double m2 = s->M2 / n;
    double m3 = s->M3 / n;
    double denom = pow(m2, 1.5);
    if (denom == 0.0) return NAN;
    return (double)(m3 / denom);
}

double online_moments_kurtosis_raw(const online_moments_t *s)
{
    if (s->n < 4) return NAN;
    double n = (double)s->n;
    double m2 = s->M2 / n;
    double m4 = s->M4 / n;
    double denom = m2 * m2;
    if (denom == 0.0) return NAN;
    /* MATLAB default raw kurtosis: m4 / m2^2 (NOT excess) */
    return (double)(m4 / denom);
}
