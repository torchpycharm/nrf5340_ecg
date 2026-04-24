#ifndef MODEL_BACKENDS_H
#define MODEL_BACKENDS_H

#include "models/model_runtime.h"

int ecg_model_backend_knn_1_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                  ecg_model_result_t *out);

int ecg_model_backend_dtree_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                  ecg_model_result_t *out);

int ecg_model_backend_svm_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                ecg_model_result_t *out);

int ecg_model_backend_bayes_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                  ecg_model_result_t *out);

int ecg_model_backend_lda_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                ecg_model_result_t *out);

int ecg_model_backend_ensemble_dtree_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                           ecg_model_result_t *out);

int ecg_model_backend_gboost_dtree_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                         ecg_model_result_t *out);

int ecg_model_backend_mix1_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                 ecg_model_result_t *out);

int ecg_model_backend_mix2_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                 ecg_model_result_t *out);

int ecg_model_backend_mix3_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                 ecg_model_result_t *out);

#endif // MODEL_BACKENDS_H
