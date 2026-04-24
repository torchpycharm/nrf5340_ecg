#include <errno.h>

#include "models/model_runtime.h"
#include "model_backends.h"

static bool model_id_valid(ecg_model_id_t model)
{
    return model > ECG_MODEL_NONE && model < ECG_MODEL_MAX;
}

int ecg_model_runtime_init(ecg_model_runtime_t *rt, ecg_model_id_t model)
{
    if (!rt || !model_id_valid(model)) {
        return -EINVAL;
    }

    rt->active_model = model;
    rt->initialized = true;
    return 0;
}

int ecg_model_runtime_set_active(ecg_model_runtime_t *rt, ecg_model_id_t model)
{
    if (!rt || !rt->initialized || !model_id_valid(model)) {
        return -EINVAL;
    }

    rt->active_model = model;
    return 0;
}

const char *ecg_model_runtime_name(ecg_model_id_t model)
{
    switch (model) {
    case ECG_MODEL_FNN_GBOOST_FLOAT:
        return "fnn_gboost_float";
    case ECG_MODEL_FNN_FLOAT:
        return "fnn_float";
    case ECG_MODEL_FNN_BAGGING_FLOAT:
        return "fnn_bagging_float";
    case ECG_MODEL_FNN_BAGGING_INT8:
        return "fnn_bagging_int8";
    case ECG_MODEL_FNN_INT8:
        return "fnn_int8";
    case ECG_MODEL_FNN_GBOOST_INT8:
        return "fnn_gboost_int8";
    case ECG_MODEL_BIN_KNN_1:
        return "bin_knn_1";
    case ECG_MODEL_BIN_DTREE:
        return "bin_dtree";
    case ECG_MODEL_BIN_SVM:
        return "bin_svm";
    case ECG_MODEL_BIN_BAYES:
        return "bin_bayes";
    case ECG_MODEL_BIN_LDA:
        return "bin_lda";
    case ECG_MODEL_BIN_ENSEMBLE_DTREE:
        return "bin_ensemble_dtree";
    case ECG_MODEL_BIN_GBOOST_DTREE:
        return "bin_gboost_dtree";
    case ECG_MODEL_BIN_MIX1:
        return "bin_mix1";
    case ECG_MODEL_BIN_MIX2:
        return "bin_mix2";
    case ECG_MODEL_BIN_MIX3:
        return "bin_mix3";
    case ECG_MODEL_BIN_I8_KNN_1:
        return "bin_i8_knn_1";
    case ECG_MODEL_BIN_I8_DTREE:
        return "bin_i8_dtree";
    case ECG_MODEL_BIN_I8_SVM:
        return "bin_i8_svm";
    case ECG_MODEL_BIN_I8_BAYES:
        return "bin_i8_bayes";
    case ECG_MODEL_BIN_I8_LDA:
        return "bin_i8_lda";
    case ECG_MODEL_BIN_I8_ENSEMBLE_DTREE:
        return "bin_i8_ensemble_dtree";
    case ECG_MODEL_BIN_I8_GBOOST_DTREE:
        return "bin_i8_gboost_dtree";
    case ECG_MODEL_BIN_I8_MIX1:
        return "bin_i8_mix1";
    case ECG_MODEL_BIN_I8_MIX2:
        return "bin_i8_mix2";
    case ECG_MODEL_BIN_I8_MIX3:
        return "bin_i8_mix3";
    default:
        return "unknown";
    }
}

int ecg_model_runtime_infer(ecg_model_runtime_t *rt,
                            const double features[ECG_MODEL_FEATURE_DIM],
                            ecg_model_result_t *out)
{
    if (!rt || !rt->initialized || !features || !out) {
        return -EINVAL;
    }

    switch (rt->active_model) {
    case ECG_MODEL_FNN_GBOOST_FLOAT:
        return ecg_model_backend_fnn_gboost_float_infer(features, out);
    case ECG_MODEL_FNN_FLOAT:
        return ecg_model_backend_fnn_float_infer(features, out);
    case ECG_MODEL_FNN_BAGGING_FLOAT:
        return ecg_model_backend_fnn_bagging_float_infer(features, out);
    case ECG_MODEL_FNN_BAGGING_INT8:
        return ecg_model_backend_fnn_bagging_int8_infer(features, out);
    case ECG_MODEL_FNN_INT8:
        return ecg_model_backend_fnn_int8_infer(features, out);
    case ECG_MODEL_FNN_GBOOST_INT8:
        return ecg_model_backend_fnn_gboost_int8_infer(features, out);
    case ECG_MODEL_BIN_KNN_1:
        return ecg_model_backend_bin_knn_1_infer(features, out);
    case ECG_MODEL_BIN_DTREE:
        return ecg_model_backend_bin_dtree_infer(features, out);
    case ECG_MODEL_BIN_SVM:
        return ecg_model_backend_bin_svm_infer(features, out);
    case ECG_MODEL_BIN_BAYES:
        return ecg_model_backend_bin_bayes_infer(features, out);
    case ECG_MODEL_BIN_LDA:
        return ecg_model_backend_bin_lda_infer(features, out);
    case ECG_MODEL_BIN_ENSEMBLE_DTREE:
        return ecg_model_backend_bin_ensemble_dtree_infer(features, out);
    case ECG_MODEL_BIN_GBOOST_DTREE:
        return ecg_model_backend_bin_gboost_dtree_infer(features, out);
    case ECG_MODEL_BIN_MIX1:
        return ecg_model_backend_bin_mix1_infer(features, out);
    case ECG_MODEL_BIN_MIX2:
        return ecg_model_backend_bin_mix2_infer(features, out);
    case ECG_MODEL_BIN_MIX3:
        return ecg_model_backend_bin_mix3_infer(features, out);
    case ECG_MODEL_BIN_I8_KNN_1:
        return ecg_model_backend_bin_i8_knn_1_infer(features, out);
    case ECG_MODEL_BIN_I8_DTREE:
        return ecg_model_backend_bin_i8_dtree_infer(features, out);
    case ECG_MODEL_BIN_I8_SVM:
        return ecg_model_backend_bin_i8_svm_infer(features, out);
    case ECG_MODEL_BIN_I8_BAYES:
        return ecg_model_backend_bin_i8_bayes_infer(features, out);
    case ECG_MODEL_BIN_I8_LDA:
        return ecg_model_backend_bin_i8_lda_infer(features, out);
    case ECG_MODEL_BIN_I8_ENSEMBLE_DTREE:
        return ecg_model_backend_bin_i8_ensemble_dtree_infer(features, out);
    case ECG_MODEL_BIN_I8_GBOOST_DTREE:
        return ecg_model_backend_bin_i8_gboost_dtree_infer(features, out);
    case ECG_MODEL_BIN_I8_MIX1:
        return ecg_model_backend_bin_i8_mix1_infer(features, out);
    case ECG_MODEL_BIN_I8_MIX2:
        return ecg_model_backend_bin_i8_mix2_infer(features, out);
    case ECG_MODEL_BIN_I8_MIX3:
        return ecg_model_backend_bin_i8_mix3_infer(features, out);
    default:
        return -ENOTSUP;
    }
}
