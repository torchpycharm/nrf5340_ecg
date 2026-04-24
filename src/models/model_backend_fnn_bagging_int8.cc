#include <errno.h>
#include <math.h>

#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "model_backends.h"
#include "models/fnn_bagging_int8_model_data.h"

#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

constexpr int kInputDim = ECG_MODEL_FEATURE_DIM;
constexpr int kNumModels = 3;
constexpr float kDecisionThreshold = 0.5f;
constexpr size_t kTensorArenaSize = 12 * 1024;

const float kMean[kInputDim] = {
    15.0445f, 16.3455f, 16.6089f, 1.4722f, 1.5089f, 1.6104f,
};

const float kScale[kInputDim] = {
    12.3295f, 11.9176f, 15.3273f, 2.5767f, 2.7871f, 2.7538f,
};

TfLiteStatus RegisterOps(tflite::MicroMutableOpResolver<2> &op_resolver)
{
    TfLiteStatus status = op_resolver.AddFullyConnected();
    if (status != kTfLiteOk) {
        return status;
    }

    status = op_resolver.AddLogistic();
    if (status != kTfLiteOk) {
        return status;
    }

    return kTfLiteOk;
}

void NormalizeInput(const double features[kInputDim], float normalized[kInputDim])
{
    for (int i = 0; i < kInputDim; i++) {
        const float feature = static_cast<float>(features[i]);
        normalized[i] = (feature - kMean[i]) / kScale[i];
    }
}

int8_t QuantizeToInt8(float value, float scale, int zero_point)
{
    const int32_t quantized = (int32_t)lroundf(value / scale) + zero_point;
    return (int8_t)CLAMP(quantized, -128, 127);
}

int RunSingleModel(const unsigned char *model_data,
                   const float input_features[kInputDim],
                   float *probability)
{
    if (!model_data || !input_features || !probability) {
        return -EINVAL;
    }

    const tflite::Model *model = tflite::GetModel(model_data);
    if (!model || model->version() != TFLITE_SCHEMA_VERSION) {
        return -EINVAL;
    }

    tflite::MicroMutableOpResolver<2> op_resolver;
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

    *probability = (float)(output->data.int8[0] - output->params.zero_point) * output->params.scale;
    *probability = CLAMP(*probability, 0.0f, 1.0f);
    return 0;
}

}  // namespace

extern "C" int ecg_model_backend_fnn_bagging_int8_infer(
    const double features[ECG_MODEL_FEATURE_DIM],
    ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    float input_norm[kInputDim];
    NormalizeInput(features, input_norm);

    const unsigned char *models[kNumModels] = {
        g_fnn_bagging_model_1_int8_tflite,
        g_fnn_bagging_model_2_int8_tflite,
        g_fnn_bagging_model_3_int8_tflite,
    };

    float probs[kNumModels] = {0.0f};
    int positive_votes = 0;

    for (int i = 0; i < kNumModels; i++) {
        const int ret = RunSingleModel(models[i], input_norm, &probs[i]);
        if (ret != 0) {
            printk("[bagging_int8] model_%d inference failed: %d\n", i + 1, ret);
            return ret;
        }

        if (probs[i] > kDecisionThreshold) {
            positive_votes++;
        }
    }

    const float avg_prob = (probs[0] + probs[1] + probs[2]) / (float)kNumModels;
    const int label = (positive_votes > (kNumModels / 2)) ? 1 : 0;

    printk("[bagging_int8] probs=[%.5f, %.5f, %.5f] votes=%d label=%d\n",
           (double)probs[0], (double)probs[1], (double)probs[2],
           positive_votes, label);

    out->label = label;
    out->score = avg_prob;
    out->supported = true;

    return 0;
}
