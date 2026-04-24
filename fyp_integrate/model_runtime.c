#include <errno.h>

#include "models/model_runtime.h"
#include "model_backends.h"

static bool model_id_valid(ecg_model_id_t model)
{
    return model > ECG_MODEL_NONE && model < ECG_MODEL_MAX;
}

int ecg_model_runtime_init(ecg_model_runtime_t *rt, ecg_model_id_t model)
{
    if (!rt || !model_id_valid(model))
    {
        return -EINVAL;
    }

    rt->active_model = model;
    rt->initialized = true;
    return 0;
}

int ecg_model_runtime_set_active(ecg_model_runtime_t *rt, ecg_model_id_t model)
{
    if (!rt || !rt->initialized || !model_id_valid(model))
    {
        return -EINVAL;
    }

    rt->active_model = model;
    return 0;
}

const char *ecg_model_runtime_name(ecg_model_id_t model)
{
    switch (model)
    {
    case ECG_MODEL_KNN_1:
        return "1knn";
    case ECG_MODEL_DTREE:
        return "2dtree";
    case ECG_MODEL_SVM:
        return "3svm";
    case ECG_MODEL_BAYES:
        return "4bayes";
    case ECG_MODEL_LDA:
        return "5lda";
    case ECG_MODEL_ENSEMBLE_DTREE:
        return "7e_dtree";
    case ECG_MODEL_GBOOST_DTREE:
        return "8g_dtree";
    case ECG_MODEL_MIX1:
        return "11mix1";
    case ECG_MODEL_MIX2:
        return "12mix2";
    case ECG_MODEL_MIX3:
        return "13mix3";
    default:
        return "unknown";
    }
}

int ecg_model_runtime_infer(ecg_model_runtime_t *rt,
                            const double features[ECG_MODEL_FEATURE_DIM],
                            ecg_model_result_t *out)
{
    if (!rt || !rt->initialized || !features || !out)
    {
        return -EINVAL;
    }

    switch (rt->active_model)
    {
    case ECG_MODEL_KNN_1:
        return ecg_model_backend_knn_1_infer(features, out);
    case ECG_MODEL_DTREE:
        return ecg_model_backend_dtree_infer(features, out);
    case ECG_MODEL_SVM:
        return ecg_model_backend_svm_infer(features, out);
    case ECG_MODEL_BAYES:
        return ecg_model_backend_bayes_infer(features, out);
    case ECG_MODEL_LDA:
        return ecg_model_backend_lda_infer(features, out);
    case ECG_MODEL_ENSEMBLE_DTREE:
        return ecg_model_backend_ensemble_dtree_infer(features, out);
    case ECG_MODEL_GBOOST_DTREE:
        return ecg_model_backend_gboost_dtree_infer(features, out);
    case ECG_MODEL_MIX1:
        return ecg_model_backend_mix1_infer(features, out);
    case ECG_MODEL_MIX2:
        return ecg_model_backend_mix2_infer(features, out);
    case ECG_MODEL_MIX3:
        return ecg_model_backend_mix3_infer(features, out);
    default:
        return -ENOTSUP;
    }
}
