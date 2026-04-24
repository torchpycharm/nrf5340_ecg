#ifndef MODEL_INT8_PARAMS_H
#define MODEL_INT8_PARAMS_H

#include <stdint.h>

#include "ecg_model_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Fixed-point formats:
// - Raw features: Q16.16 in int32_t
// - mean: Q16.16
// - inv_std: Q2.30 (approx)
// - inv_feat_scale: Q2.30 (approx), where x_q = round(x_std / feat_scale)
// - feat_scale_sq: Q16.16 (feat_scale^2)
// - SVM alpha: Q2.14
// - SVM kernel: Q1.15 (0..1)
// - SVM bias: Q2.14
// - NB logs: Q12.20
// - LDA weights/bias: Q2.14

typedef struct ecg_dtree_node_i8_t
{
    uint16_t node_id;
    uint8_t is_branch;
    uint8_t cut_pred_idx; // 1-based
    int8_t cut_point_q;
    uint8_t node_class; // 0/1
    uint16_t left_child;
    uint16_t right_child;
} ecg_dtree_node_i8_t;

typedef struct ecg_gbtree_node_i8_t
{
    uint16_t node_id;
    uint8_t is_branch;
    uint8_t cut_pred_idx; // 1-based
    int8_t cut_point_q;
    int8_t leaf_value_q; // scaled (sum sign only)
    uint16_t left_child;
    uint16_t right_child;
} ecg_gbtree_node_i8_t;

// Standardization + quantization params
extern const int32_t g_mean_q16[FEAT_DIM];
extern const int32_t g_inv_std_q16[FEAT_DIM];
extern const int32_t g_inv_feat_scale_q16[FEAT_DIM];
extern const int32_t g_feat_scale_sq_q16[FEAT_DIM];

// KNN
extern const int8_t g_train_X_q[TRAIN_NUM][FEAT_DIM];
extern const uint8_t g_train_Y[TRAIN_NUM];

// DTrees
extern const ecg_dtree_node_i8_t g_dtree_nodes_q[MAX_NODES];
extern const int g_dtree_num_nodes;

extern const ecg_dtree_node_i8_t g_7e_nodes1_q[MAX_NODES];
extern const int g_i8_7e_num_nodes1;
extern const ecg_dtree_node_i8_t g_7e_nodes2_q[MAX_NODES];
extern const int g_i8_7e_num_nodes2;
extern const ecg_dtree_node_i8_t g_7e_nodes3_q[MAX_NODES];
extern const int g_i8_7e_num_nodes3;

// GBoost trees
extern const ecg_gbtree_node_i8_t g_8g_nodes1_q[MAX_NODES];
extern const int g_i8_8g_num_nodes1;
extern const ecg_gbtree_node_i8_t g_8g_nodes2_q[MAX_NODES];
extern const int g_i8_8g_num_nodes2;
extern const ecg_gbtree_node_i8_t g_8g_nodes3_q[MAX_NODES];
extern const int g_i8_8g_num_nodes3;

// SVM (RBF) params
extern const int g_svm_sv_num;
extern const int8_t g_svm_support_vec_q[200][FEAT_DIM];
extern const int16_t g_svm_alpha_q14[200];
extern const int8_t g_svm_label_sign[200]; // +1/-1
extern const int16_t g_svm_bias_q14;

// exp(-t) LUT for t in [0, 16] with step 1/256, Q1.15
extern const int g_svm_exp_lut_size;
extern const int16_t g_svm_exp_lut_q15[4097];

// Naive Bayes (Gaussian) params (2 classes)
extern const int8_t g_nb_mu_xq[2][FEAT_DIM];
extern const int32_t g_nb_K_q20[2][FEAT_DIM];
extern const int32_t g_nb_const_q20[2][FEAT_DIM];
extern const int32_t g_nb_logprior_q20[2];

// LDA params (2 classes)
extern const int16_t g_lda_w_q14[2][FEAT_DIM];
extern const int32_t g_lda_bias_q14[2];

#ifdef __cplusplus
}
#endif

#endif // MODEL_INT8_PARAMS_H
