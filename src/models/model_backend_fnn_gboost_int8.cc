#include <errno.h>
#include <math.h>
#include <stdint.h>

#include <zephyr/sys/printk.h>

#include "model_backends.h"
#include "models/fnn_gboost_int8_model_data.h"

#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

constexpr int kInputDim = ECG_MODEL_FEATURE_DIM;
constexpr int kNumModels = 3;
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
        normalized[i] = ((float)features[i] - kMean[i]) / kScale[i];
    }
}

int8_t QuantizeToInt8(float value, float scale, int zero_point)
{
    int32_t q = (int32_t)lroundf(value / scale) + zero_point;
    if (q > 127) {
        q = 127;
    } else if (q < -128) {
        q = -128;
    }

    return (int8_t)q;
}

int RunSingleModel(const unsigned char *model_data,
                   const float input_features[kInputDim],
                   float *residual)
{
    if (!model_data || !input_features || !residual) {
        return -EINVAL;
    }

    const tflite::Model *model = tflite::GetModel(model_data);
    if (!model || model->version() != TFLITE_SCHEMA_VERSION) {
        return -EINVAL;
    }

    tflite::MicroMutableOpResolver<1> op_resolver;
    if (RegisterOps(op_resolver) != kTfLiteOk) {
        return -ENOTSUP;
    }

    static uint8_t tensor_arena[kTensorArenaSize];
    tflite::MicroInterpreter interpreter(
        model, op_resolver, tensor_arena, kTensorArenaSize);

    if (interpreter.AllocateTensors() != kTfLiteOk) {
        return -ENOMEM;
    }

    TfLiteTensor *input = interpreter.input(0);
    TfLiteTensor *output = interpreter.output(0);
    if (!input || !output || input->type != kTfLiteInt8 || output->type != kTfLiteInt8) {
        return -EINVAL;
    }

    if (input->params.scale <= 0.0f || output->params.scale <= 0.0f) {
        return -EINVAL;
    }

    for (int i = 0; i < kInputDim; i++) {
        input->data.int8[i] = QuantizeToInt8(
            input_features[i], input->params.scale, input->params.zero_point);
    }

    if (interpreter.Invoke() != kTfLiteOk) {
        return -EIO;
    }

    *residual = (float)(output->data.int8[0] - output->params.zero_point) * output->params.scale;
    return 0;
}

}  // namespace

extern "C" int ecg_model_backend_fnn_gboost_int8_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    float input_norm[kInputDim];
    NormalizeInput(features, input_norm);

    const unsigned char *models[kNumModels] = {
        g_gradient_boost_model_1_int8_tflite,
        g_gradient_boost_model_2_int8_tflite,
        g_gradient_boost_model_3_int8_tflite,
    };

    float residuals[kNumModels] = {0.0f};
    float total_pred = kBasePrediction;

    for (int i = 0; i < kNumModels; i++) {
        const int ret = RunSingleModel(models[i], input_norm, &residuals[i]);
        if (ret != 0) {
            printk("[gboost_int8] model_%d inference failed: %d\n", i + 1, ret);
            return ret;
        }

        total_pred += residuals[i];
    }

    out->score = total_pred;
    out->label = (total_pred > 0.5f) ? 1 : 0;
    out->supported = true;

    printk("[gboost_int8] residuals=[%.6f, %.6f, %.6f] score=%.6f label=%d\n",
           (double)residuals[0], (double)residuals[1], (double)residuals[2],
           (double)total_pred, out->label);

    return 0;
}
