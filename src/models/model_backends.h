#ifndef ECG_MODEL_BACKENDS_H
#define ECG_MODEL_BACKENDS_H

#include "models/model_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

int ecg_model_backend_fnn_gboost_float_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_fnn_float_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_fnn_bagging_float_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_fnn_bagging_int8_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_fnn_int8_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_fnn_gboost_int8_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);

int ecg_model_backend_bin_knn_1_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_dtree_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_svm_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_bayes_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_lda_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_ensemble_dtree_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_gboost_dtree_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_mix1_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_mix2_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_mix3_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);

int ecg_model_backend_bin_i8_knn_1_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_i8_dtree_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_i8_svm_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_i8_bayes_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_i8_lda_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_i8_ensemble_dtree_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_i8_gboost_dtree_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_i8_mix1_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_i8_mix2_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);
int ecg_model_backend_bin_i8_mix3_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ECG_MODEL_BACKENDS_H */
