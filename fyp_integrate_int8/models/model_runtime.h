#ifndef MODELS_MODEL_RUNTIME_H
#define MODELS_MODEL_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "../ecg_model_config.h"
#include "../utils_int8.h"

typedef enum ecg_model_id_t {
    ECG_MODEL_NONE = 0,
    // INT8-only models (10 total)
    ECG_MODEL_KNN_1 = 1,
    ECG_MODEL_DTREE = 2,
    ECG_MODEL_SVM = 3,
    ECG_MODEL_BAYES = 4,
    ECG_MODEL_LDA = 5,
    ECG_MODEL_ENSEMBLE_DTREE = 6, // 7e_dtree (3 trees, median)
    ECG_MODEL_GBOOST_DTREE = 7,   // 8g_dtree (3 trees, additive)
    ECG_MODEL_MIX1 = 8,           // 11mix1
    ECG_MODEL_MIX2 = 9,           // 12mix2
    ECG_MODEL_MIX3 = 10,          // 13mix3 (vote fusion)
    ECG_MODEL_MAX
} ecg_model_id_t;

typedef struct ecg_model_runtime_t {
    ecg_model_id_t active_model;
    bool initialized;
} ecg_model_runtime_t;

int ecg_model_runtime_init(ecg_model_runtime_t *rt, ecg_model_id_t model);
int ecg_model_runtime_set_active(ecg_model_runtime_t *rt, ecg_model_id_t model);
const char *ecg_model_runtime_name(ecg_model_id_t model);
int ecg_model_runtime_infer(ecg_model_runtime_t *rt,
                            const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                            ecg_model_result_t *out);

#endif // MODELS_MODEL_RUNTIME_H
