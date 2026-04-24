#include <errno.h>

#include "model_backends.h"

#include "../../fyp_integrate/model_params.h"
#include "../../fyp_integrate/utils.h"

static int label_to_out(int label, ecg_model_result_t *out)
{
    if (!out) {
        return -EINVAL;
    }

    out->label = label;
    out->score = (float)label;
    out->supported = true;
    return 0;
}

int ecg_model_backend_bin_knn_1_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                      ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    const int label = knn_predict(features);
    if (label < 0) {
        return -EIO;
    }

    return label_to_out(label, out);
}

int ecg_model_backend_bin_dtree_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                      ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    const int label = dtree_predict(features);
    if (label < 0) {
        return -EIO;
    }

    return label_to_out(label, out);
}

int ecg_model_backend_bin_svm_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                    ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    const int label = svm_predict(&g_svm_model, features);
    if (label < 0) {
        return -EIO;
    }

    return label_to_out(label, out);
}

int ecg_model_backend_bin_bayes_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                      ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    const int label = (int)nbPredict(&g_nb_params, features);
    return label_to_out(label, out);
}

int ecg_model_backend_bin_lda_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                    ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    const int label = (int)ldaPredict(&g_lda_params, features);
    return label_to_out(label, out);
}

int ecg_model_backend_bin_ensemble_dtree_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                               ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    const int pred1 = dtree_predict_choice(g_7e_nodes1, g_7e_num_nodes1, features);
    const int pred2 = dtree_predict_choice(g_7e_nodes2, g_7e_num_nodes2, features);
    const int pred3 = dtree_predict_choice(g_7e_nodes3, g_7e_num_nodes3, features);
    if (pred1 < 0 || pred2 < 0 || pred3 < 0) {
        return -EIO;
    }

    const int label = getMedian(pred1, pred2, pred3);
    return label_to_out(label, out);
}

static int gbtree_predict_value(const ecg_gbtree_node_t *nodes,
                                int num_nodes,
                                const double features[ECG_MODEL_FEATURE_DIM],
                                double *out_value)
{
    if (!nodes || num_nodes <= 0 || !features || !out_value) {
        return -EINVAL;
    }

    double x_std[FEAT_DIM];
    standardize_features(features, x_std);

    int current_node_id = 1;
    for (int steps = 0; steps < num_nodes + 2; steps++) {
        const int idx = current_node_id - 1;
        if (idx < 0 || idx >= num_nodes) {
            return -ERANGE;
        }

        const ecg_gbtree_node_t *node = &nodes[idx];
        if (node->is_branch == 0) {
            *out_value = node->leaf_value;
            return 0;
        }

        const int feat_idx = node->cut_pred_idx - 1;
        if (feat_idx < 0 || feat_idx >= FEAT_DIM) {
            return -ERANGE;
        }

        current_node_id = (x_std[feat_idx] <= node->cut_point) ? node->left_child : node->right_child;
    }

    return -EIO;
}

int ecg_model_backend_bin_gboost_dtree_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                             ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    double v1 = 0.0;
    double v2 = 0.0;
    double v3 = 0.0;
    if (gbtree_predict_value(g_8g_nodes1, g_8g_num_nodes1, features, &v1) != 0 ||
        gbtree_predict_value(g_8g_nodes2, g_8g_num_nodes2, features, &v2) != 0 ||
        gbtree_predict_value(g_8g_nodes3, g_8g_num_nodes3, features, &v3) != 0) {
        return -EIO;
    }

    const double raw_score = 0.5 + v1 + v2 + v3;
    out->label = gboosting_predict(v1, v2, v3);
    out->score = (float)raw_score;
    out->supported = true;
    return 0;
}

int ecg_model_backend_bin_mix1_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                     ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    const int pred2 = ecg_model_backend_bin_dtree_infer(features, out) == 0 ? out->label : -1;
    const int pred4 = ecg_model_backend_bin_bayes_infer(features, out) == 0 ? out->label : -1;
    const int pred5 = ecg_model_backend_bin_lda_infer(features, out) == 0 ? out->label : -1;
    if (pred2 < 0 || pred4 < 0 || pred5 < 0) {
        return -EIO;
    }

    const int label = fuseSingleSample1(pred2, pred4, pred5);
    return label_to_out(label, out);
}

int ecg_model_backend_bin_mix2_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                     ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    const int pred4 = ecg_model_backend_bin_bayes_infer(features, out) == 0 ? out->label : -1;
    const int pred5 = ecg_model_backend_bin_lda_infer(features, out) == 0 ? out->label : -1;
    if (pred4 < 0 || pred5 < 0) {
        return -EIO;
    }

    const int label = fuseSingleSample2(pred4, pred5);
    return label_to_out(label, out);
}

int ecg_model_backend_bin_mix3_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                     ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    const int pred1 = ecg_model_backend_bin_knn_1_infer(features, out) == 0 ? out->label : -1;
    const int pred2 = ecg_model_backend_bin_dtree_infer(features, out) == 0 ? out->label : -1;
    const int pred3 = ecg_model_backend_bin_svm_infer(features, out) == 0 ? out->label : -1;
    const int pred4 = ecg_model_backend_bin_bayes_infer(features, out) == 0 ? out->label : -1;
    const int pred5 = ecg_model_backend_bin_lda_infer(features, out) == 0 ? out->label : -1;
    const int pred7 = ecg_model_backend_bin_ensemble_dtree_infer(features, out) == 0 ? out->label : -1;
    if (pred1 < 0 || pred2 < 0 || pred3 < 0 || pred4 < 0 || pred5 < 0 || pred7 < 0) {
        return -EIO;
    }

    const int label = modelVoteFusion(pred1, pred2, pred3, pred4, pred5, pred7);
    return label_to_out(label, out);
}
