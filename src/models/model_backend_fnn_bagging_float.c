#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "model_backends.h"

LOG_MODULE_REGISTER(model_fnn_bag_f, LOG_LEVEL_INF);

int ecg_model_backend_fnn_bagging_float_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out)
{
    ARG_UNUSED(features);

    if (!out) {
        return -EINVAL;
    }

    out->label = 0;
    out->score = 0.0f;
    out->supported = false;

    LOG_WRN("Fnn_bagging float backend is not integrated yet");
    return -ENOTSUP;
}
