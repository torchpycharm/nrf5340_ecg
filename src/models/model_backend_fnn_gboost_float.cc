#include <errno.h>

#include <zephyr/sys/printk.h>

#include "model_backends.h"
#include "models/fnn_gboost_model_data.h"

#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

constexpr int kInputDim = ECG_MODEL_FEATURE_DIM;
constexpr float kBasePrediction = 0.5f;
constexpr size_t kTensorArenaSize = 12 * 1024;

const float kMean[kInputDim] = {
    15.0445f, 16.3455f, 16.6089f, 1.4722f, 1.5089f, 1.6104f,
};

const float kScale[kInputDim] = {
    12.3295f, 11.9176f, 15.3273f, 2.5767f, 2.7871f, 2.7538f,
};

TfLiteStatus RegisterOps(tflite::MicroMutableOpResolver<1> &op_resolver)
{
    return op_resolver.AddFullyConnected();
}

void NormalizeInput(const double features[kInputDim], float normalized[kInputDim])
{
    for (int i = 0; i < kInputDim; i++) {
        const float feature = static_cast<float>(features[i]);
        normalized[i] = (feature - kMean[i]) / kScale[i];
    }
}

float RunSingleModel(const unsigned char *model_data,
                     const float input_features[kInputDim],
                     int *err)
{
    const tflite::Model *model = tflite::GetModel(model_data);
    if (!model || model->version() != TFLITE_SCHEMA_VERSION) {
        *err = -EINVAL;
        return 0.0f;
    }

    tflite::MicroMutableOpResolver<1> op_resolver;
    if (RegisterOps(op_resolver) != kTfLiteOk) {
        *err = -ENOTSUP;
        return 0.0f;
    }

    static uint8_t tensor_arena[kTensorArenaSize];
    tflite::MicroInterpreter interpreter(
        model, op_resolver, tensor_arena, kTensorArenaSize);

    if (interpreter.AllocateTensors() != kTfLiteOk) {
        *err = -ENOMEM;
        return 0.0f;
    }

    TfLiteTensor *input = interpreter.input(0);
    TfLiteTensor *output = interpreter.output(0);
    if (!input || !output || input->type != kTfLiteFloat32 || output->type != kTfLiteFloat32) {
        *err = -EINVAL;
        return 0.0f;
    }

    for (int i = 0; i < kInputDim; i++) {
        input->data.f[i] = input_features[i];
    }

    if (interpreter.Invoke() != kTfLiteOk) {
        *err = -EIO;
        return 0.0f;
    }

    return output->data.f[0];
}

}  // namespace

extern "C" int ecg_model_backend_fnn_gboost_float_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    float input_norm[kInputDim];
    NormalizeInput(features, input_norm);

    int err = 0;
    float total_pred = kBasePrediction;

    total_pred += RunSingleModel(g_gradient_boost_model_1_tflite, input_norm, &err);
    if (err != 0) {
        printk("[gboost] model_1 inference failed: %d\n", err);
        return err;
    }

    total_pred += RunSingleModel(g_gradient_boost_model_2_tflite, input_norm, &err);
    if (err != 0) {
        printk("[gboost] model_2 inference failed: %d\n", err);
        return err;
    }

    total_pred += RunSingleModel(g_gradient_boost_model_3_tflite, input_norm, &err);
    if (err != 0) {
        printk("[gboost] model_3 inference failed: %d\n", err);
        return err;
    }

    printk("[gboost] input_norm=[%.5f, %.5f, %.5f, %.5f, %.5f, %.5f]\n",
           (double)input_norm[0], (double)input_norm[1], (double)input_norm[2],
           (double)input_norm[3], (double)input_norm[4], (double)input_norm[5]);
    printk("[gboost] score=%.6f label=%d (threshold=0.5)\n",
           (double)total_pred,
           (total_pred > 0.5f) ? 1 : 0);

    out->score = total_pred;
    out->label = (total_pred > 0.5f) ? 1 : 0;
    out->supported = true;

    return 0;
}
