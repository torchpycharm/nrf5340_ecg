#ifndef ECG_MODEL_BACKENDS_H
#define ECG_MODEL_BACKENDS_H

#include "models/model_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

int ecg_model_backend_param_binary_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                         ecg_model_result_t *out);
int ecg_model_backend_fnn_gboost_float_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_fnn_bagging_float_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ECG_MODEL_BACKENDS_H */
