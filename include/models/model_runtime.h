#ifndef ECG_MODEL_RUNTIME_H
#define ECG_MODEL_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ECG_MODEL_FEATURE_DIM 6

typedef enum {
    ECG_MODEL_NONE = 0,
    ECG_MODEL_FNN_GBOOST_FLOAT,
    ECG_MODEL_FNN_FLOAT,
    ECG_MODEL_FNN_BAGGING_FLOAT,
    ECG_MODEL_FNN_BAGGING_INT8,
    ECG_MODEL_FNN_INT8,
    ECG_MODEL_FNN_GBOOST_INT8,
    ECG_MODEL_BIN_KNN_1,
    ECG_MODEL_BIN_DTREE,
    ECG_MODEL_BIN_SVM,
    ECG_MODEL_BIN_BAYES,
    ECG_MODEL_BIN_LDA,
    ECG_MODEL_BIN_ENSEMBLE_DTREE,
    ECG_MODEL_BIN_GBOOST_DTREE,
    ECG_MODEL_BIN_MIX1,
    ECG_MODEL_BIN_MIX2,
    ECG_MODEL_BIN_MIX3,
    ECG_MODEL_BIN_I8_KNN_1,
    ECG_MODEL_BIN_I8_DTREE,
    ECG_MODEL_BIN_I8_SVM,
    ECG_MODEL_BIN_I8_BAYES,
    ECG_MODEL_BIN_I8_LDA,
    ECG_MODEL_BIN_I8_ENSEMBLE_DTREE,
    ECG_MODEL_BIN_I8_GBOOST_DTREE,
    ECG_MODEL_BIN_I8_MIX1,
    ECG_MODEL_BIN_I8_MIX2,
    ECG_MODEL_BIN_I8_MIX3,
    ECG_MODEL_MAX
} ecg_model_id_t;

typedef struct {
    int32_t label;   /* 1=valid, 0=invalid */
    float score;     /* model raw score or confidence */
    bool supported;  /* false means backend is not implemented on this target */
} ecg_model_result_t;

typedef struct {
    ecg_model_id_t active_model;
    bool initialized;
} ecg_model_runtime_t;

int ecg_model_runtime_init(ecg_model_runtime_t *rt, ecg_model_id_t model);
int ecg_model_runtime_set_active(ecg_model_runtime_t *rt, ecg_model_id_t model);
const char *ecg_model_runtime_name(ecg_model_id_t model);
int ecg_model_runtime_infer(ecg_model_runtime_t *rt,
                            const double features[ECG_MODEL_FEATURE_DIM],
                            ecg_model_result_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ECG_MODEL_RUNTIME_H */
