#include "utils.h"

static const double k_mean_all[FEAT_DIM] = {
    15.11029151f, 16.26610535f, 16.65656101f,
    1.43154793f, 1.44921991f, 1.56615979f,
};

static const double k_std_all[FEAT_DIM] = {
    12.72234190f, 11.67055237f, 15.89695784f,
    2.60421645f, 2.80454207f, 2.77770177f,
};
/*
 * 标准化输入特征：Z-score
 * x_norm = (x - mean) / std
 */
void standardize_features(const double *x_in, double *x_out)
{
    for (int i = 0; i < FEAT_DIM; i++)
    {
        if (k_std_all[i] != 0)
            x_out[i] = (x_in[i] - k_mean_all[i]) / k_std_all[i];
        else
            x_out[i] = x_in[i];
    }
}