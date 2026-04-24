#include <errno.h>
#include <limits.h>
#include <stdint.h>

#include "model_backends.h"
#include "model_int8_params.h"
#include "utils_int8.h"

// exp(-t) LUT covers t in [0, 16] with step 1/256 (t is Q16.16)
#define SVM_EXP_TMAX_Q16 (16 << 16)

static int label_to_out(int label, ecg_model_result_t *out)
{
    if (!out)
        return -EINVAL;
    out->label = label;
    out->score_q = 0;
    return 0;
}

static inline int32_t abs_i32(int32_t x)
{
    return (x < 0) ? -x : x;
}

// ---------------- KNN ----------------
int ecg_model_backend_knn_1_infer(const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                                  ecg_model_result_t *out)
{
    if (!features_raw || !out)
        return -EINVAL;

    int8_t xq[FEAT_DIM];
    ecg_standardize_and_quantize_int8(features_raw, xq);

    int32_t best_dist = INT32_MAX;
    int best_label = -1;
    for (int i = 0; i < TRAIN_NUM; i++)
    {
        int32_t dist = 0;
        for (int d = 0; d < FEAT_DIM; d++)
        {
            int32_t diff = (int32_t)xq[d] - (int32_t)g_train_X_q[i][d];
            dist += diff * diff;
        }
        if (dist < best_dist)
        {
            best_dist = dist;
            best_label = (int)g_train_Y[i];
        }
    }
    return label_to_out(best_label, out);
}

// ---------------- DTREE core ----------------
static int dtree_predict_int8(const ecg_dtree_node_i8_t *nodes,
                              int num_nodes,
                              const int8_t xq[FEAT_DIM])
{
    if (!nodes || num_nodes <= 0 || !xq)
        return -EINVAL;

    int current_node_id = 1;
    for (int steps = 0; steps < num_nodes + 2; steps++)
    {
        int idx = current_node_id - 1;
        if (idx < 0 || idx >= num_nodes)
            return -ERANGE;
        const ecg_dtree_node_i8_t *n = &nodes[idx];
        if (n->is_branch == 0)
            return (int)n->node_class;

        int feat_idx = (int)n->cut_pred_idx - 1;
        if (feat_idx < 0 || feat_idx >= FEAT_DIM)
            return -ERANGE;

        current_node_id = (xq[feat_idx] <= n->cut_point_q) ? (int)n->left_child : (int)n->right_child;
    }
    return -EIO;
}

int ecg_model_backend_dtree_infer(const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                                  ecg_model_result_t *out)
{
    if (!features_raw || !out)
        return -EINVAL;

    int8_t xq[FEAT_DIM];
    ecg_standardize_and_quantize_int8(features_raw, xq);

    int label = dtree_predict_int8(g_dtree_nodes_q, g_dtree_num_nodes, xq);
    if (label < 0)
        return -EIO;
    return label_to_out(label, out);
}

// ---------------- SVM (INT-only approx) ----------------
// Compute dist_sq = sum( (diff_q^2) * feat_scale^2 ) in Q16.16
static int32_t svm_dist_sq_q16(const int8_t xq[FEAT_DIM], const int8_t svq[FEAT_DIM])
{
    int64_t acc_q16 = 0;
    for (int d = 0; d < FEAT_DIM; d++)
    {
        int32_t diff = (int32_t)xq[d] - (int32_t)svq[d];
        int32_t diff2 = diff * diff; // up to ~ (255^2)=65025
        acc_q16 += (int64_t)diff2 * (int64_t)g_feat_scale_sq_q16[d];
    }
    if (acc_q16 > INT32_MAX)
        return INT32_MAX;
    return (int32_t)acc_q16;
}

// Approx exp(-t) where t is in Q16.16, assumes t in [0,16]
static int16_t svm_exp_neg_q15(int32_t t_q16)
{
    if (t_q16 <= 0)
        return 32767;
    // clamp to tmax
    if (t_q16 >= SVM_EXP_TMAX_Q16)
        return 0;

    // index = round(t / step) = round( t * 256 )
    // since step=1/256, and t is Q16.16: t * 256 -> Q16.16 * 256 = Q24.16
    int32_t idx_q16 = t_q16 * 256;
    int32_t idx = (idx_q16 + (1 << 15)) >> 16;
    if (idx < 0)
        idx = 0;
    if (idx >= g_svm_exp_lut_size)
        idx = g_svm_exp_lut_size - 1;
    return g_svm_exp_lut_q15[idx];
}

int ecg_model_backend_svm_infer(const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                                ecg_model_result_t *out)
{
    if (!features_raw || !out)
        return -EINVAL;

    int8_t xq[FEAT_DIM];
    ecg_standardize_and_quantize_int8(features_raw, xq);

    // decision in Q2.14
    int64_t decision_q14 = 0;
    int sv_num = g_svm_sv_num;
    if (sv_num <= 0 || sv_num > 200)
        return -ERANGE;

    for (int i = 0; i < sv_num; i++)
    {
        int32_t dist_q16 = svm_dist_sq_q16(xq, g_svm_support_vec_q[i]);
        int16_t k_q15 = svm_exp_neg_q15(dist_q16); // gamma assumed 1.0

        // alpha_q14 * k_q15 -> Q(14+15)=Q29, >>15 -> Q14
        int32_t contrib_q14 = (int32_t)(((int64_t)g_svm_alpha_q14[i] * (int64_t)k_q15 + (1LL << 14)) >> 15);
        if (g_svm_label_sign[i] < 0)
            contrib_q14 = -contrib_q14;
        decision_q14 += contrib_q14;
    }
    decision_q14 += g_svm_bias_q14;

    int label = (decision_q14 >= 0) ? 1 : 0;
    return label_to_out(label, out);
}

// ---------------- Naive Bayes (log Gaussian, INT-only) ----------------
int ecg_model_backend_bayes_infer(const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                                  ecg_model_result_t *out)
{
    if (!features_raw || !out)
        return -EINVAL;

    int8_t xq[FEAT_DIM];
    ecg_standardize_and_quantize_int8(features_raw, xq);

    // score in Q12.20
    int32_t best_score = INT32_MIN;
    int best_k = 0;
    for (int k = 0; k < 2; k++)
    {
        int64_t score_q20 = g_nb_logprior_q20[k];
        for (int d = 0; d < FEAT_DIM; d++)
        {
            int32_t diff = (int32_t)xq[d] - (int32_t)g_nb_mu_xq[k][d];
            int32_t diff2 = diff * diff;
            // diff2 * K (Q20) => Q20, add const Q20
            int64_t term = -((int64_t)diff2 * (int64_t)g_nb_K_q20[k][d]);
            term += g_nb_const_q20[k][d];
            score_q20 += term;
        }
        if (score_q20 > best_score)
        {
            best_score = (int32_t)score_q20;
            best_k = k;
        }
    }
    return label_to_out(best_k, out);
}

// ---------------- LDA (linear, INT-only) ----------------
int ecg_model_backend_lda_infer(const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                                ecg_model_result_t *out)
{
    if (!features_raw || !out)
        return -EINVAL;

    int8_t xq[FEAT_DIM];
    ecg_standardize_and_quantize_int8(features_raw, xq);

    // score in Q2.14 (w is Q14, xq is int8)
    int64_t best_score = INT64_MIN;
    int best_k = 0;
    for (int k = 0; k < 2; k++)
    {
        int64_t score_q14 = g_lda_bias_q14[k];
        for (int d = 0; d < FEAT_DIM; d++)
        {
            score_q14 += (int64_t)g_lda_w_q14[k][d] * (int64_t)xq[d];
        }
        if (score_q14 > best_score)
        {
            best_score = score_q14;
            best_k = k;
        }
    }
    return label_to_out(best_k, out);
}

// ---------------- Ensemble DTrees (median) ----------------
static int median3(int a, int b, int c)
{
    if ((a >= b && a <= c) || (a <= b && a >= c))
        return a;
    if ((b >= a && b <= c) || (b <= a && b >= c))
        return b;
    return c;
}

int ecg_model_backend_ensemble_dtree_infer(const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                                           ecg_model_result_t *out)
{
    if (!features_raw || !out)
        return -EINVAL;

    int8_t xq[FEAT_DIM];
    ecg_standardize_and_quantize_int8(features_raw, xq);

    int p1 = dtree_predict_int8(g_7e_nodes1_q, g_7e_num_nodes1, xq);
    int p2 = dtree_predict_int8(g_7e_nodes2_q, g_7e_num_nodes2, xq);
    int p3 = dtree_predict_int8(g_7e_nodes3_q, g_7e_num_nodes3, xq);
    if (p1 < 0 || p2 < 0 || p3 < 0)
        return -EIO;
    return label_to_out(median3(p1, p2, p3), out);
}

// ---------------- GBoost DTrees (additive) ----------------
static int gbtree_predict_leaf_q(const ecg_gbtree_node_i8_t *nodes, int num_nodes, const int8_t xq[FEAT_DIM], int *out_leaf_q)
{
    if (!nodes || num_nodes <= 0 || !xq || !out_leaf_q)
        return -EINVAL;

    int current_node_id = 1;
    for (int steps = 0; steps < num_nodes + 2; steps++)
    {
        int idx = current_node_id - 1;
        if (idx < 0 || idx >= num_nodes)
            return -ERANGE;
        const ecg_gbtree_node_i8_t *n = &nodes[idx];
        if (n->is_branch == 0)
        {
            *out_leaf_q = (int)n->leaf_value_q;
            return 0;
        }
        int feat_idx = (int)n->cut_pred_idx - 1;
        if (feat_idx < 0 || feat_idx >= FEAT_DIM)
            return -ERANGE;
        current_node_id = (xq[feat_idx] <= n->cut_point_q) ? (int)n->left_child : (int)n->right_child;
    }
    return -EIO;
}

int ecg_model_backend_gboost_dtree_infer(const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                                         ecg_model_result_t *out)
{
    if (!features_raw || !out)
        return -EINVAL;

    int8_t xq[FEAT_DIM];
    ecg_standardize_and_quantize_int8(features_raw, xq);

    int v1 = 0, v2 = 0, v3 = 0;
    if (gbtree_predict_leaf_q(g_8g_nodes1_q, g_8g_num_nodes1, xq, &v1) != 0 ||
        gbtree_predict_leaf_q(g_8g_nodes2_q, g_8g_num_nodes2, xq, &v2) != 0 ||
        gbtree_predict_leaf_q(g_8g_nodes3_q, g_8g_num_nodes3, xq, &v3) != 0)
        return -EIO;

    // Original: label = (0.5 + v1 + v2 + v3 > 0.5)
    // Our leaf_q uses 0.5 -> 127, so compare (127 + v1 + v2 + v3 > 127)
    int sum = 127 + v1 + v2 + v3;
    int label = (sum > 127) ? 1 : 0;
    return label_to_out(label, out);
}

// ---------------- Mixes ----------------
int ecg_model_backend_mix1_infer(const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                                 ecg_model_result_t *out)
{
    if (!features_raw || !out)
        return -EINVAL;
    ecg_model_result_t tmp;
    int p2 = (ecg_model_backend_dtree_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    int p4 = (ecg_model_backend_bayes_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    int p5 = (ecg_model_backend_lda_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    if (p2 < 0 || p4 < 0 || p5 < 0)
        return -EIO;

    // logic from 11mix1.c
    int result = 0;
    if (p2 == 1)
    {
        if (p4 == 0)
        {
            result = (p5 == 1) ? 0 : 1;
        }
        else
        {
            result = 1;
        }
    }
    return label_to_out(result, out);
}

int ecg_model_backend_mix2_infer(const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                                 ecg_model_result_t *out)
{
    if (!features_raw || !out)
        return -EINVAL;
    ecg_model_result_t tmp;
    int p4 = (ecg_model_backend_bayes_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    int p5 = (ecg_model_backend_lda_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    if (p4 < 0 || p5 < 0)
        return -EIO;

    // logic from 12mix2.c
    int result = 0;
    if (p4 == 0)
        result = 0;
    else
        result = (p5 == 1) ? 1 : 0;
    return label_to_out(result, out);
}

int ecg_model_backend_mix3_infer(const int32_t features_raw[ECG_MODEL_FEATURE_DIM],
                                 ecg_model_result_t *out)
{
    if (!features_raw || !out)
        return -EINVAL;
    ecg_model_result_t tmp;
    int p1 = (ecg_model_backend_knn_1_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    int p2 = (ecg_model_backend_dtree_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    int p3 = (ecg_model_backend_svm_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    int p4 = (ecg_model_backend_bayes_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    int p5 = (ecg_model_backend_lda_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    int p7 = (ecg_model_backend_ensemble_dtree_infer(features_raw, &tmp) == 0) ? tmp.label : -1;
    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0 || p7 < 0)
        return -EIO;

    int sum_total = p1 + p2 + p3 + p4 + p5 + p7;
    // mean > 0.5  <=> sum_total > 3
    if (sum_total > 3)
        return label_to_out(1, out);
    if (sum_total < 3)
        return label_to_out(0, out);

    // tie: use models 1/3/7 (per 13mix3.c)
    int sum_sub = p1 + p3 + p7;
    return label_to_out((sum_sub > 1) ? 1 : 0, out);
}
