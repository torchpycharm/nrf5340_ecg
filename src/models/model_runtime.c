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
    case ECG_MODEL_PARAM_BINARY:
        return "param_binary";
    case ECG_MODEL_FNN_BAGGING_FLOAT:
        return "fnn_bagging_float";
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
    case ECG_MODEL_PARAM_BINARY:
        return ecg_model_backend_param_binary_infer(features, out);
    case ECG_MODEL_FNN_BAGGING_FLOAT:
        return ecg_model_backend_fnn_bagging_float_infer(features, out);
    default:
        return -ENOTSUP;
    }
}
