#ifndef MODEL_PARAMS_H
#define MODEL_PARAMS_H

#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ecg_gbtree_node_t
{
    int node_id;
    int is_branch;
    int cut_pred_idx;
    double cut_point;
    double leaf_value;
    int left_child;
    int right_child;
} ecg_gbtree_node_t;

extern const SVMModel g_svm_model;
extern const NBParams g_nb_params;
extern const LDAParams g_lda_params;

extern const DTreeNode g_7e_nodes1[MAX_NODES];
extern const int g_7e_num_nodes1;
extern const DTreeNode g_7e_nodes2[MAX_NODES];
extern const int g_7e_num_nodes2;
extern const DTreeNode g_7e_nodes3[MAX_NODES];
extern const int g_7e_num_nodes3;

extern const ecg_gbtree_node_t g_8g_nodes1[MAX_NODES];
extern const int g_8g_num_nodes1;
extern const ecg_gbtree_node_t g_8g_nodes2[MAX_NODES];
extern const int g_8g_num_nodes2;
extern const ecg_gbtree_node_t g_8g_nodes3[MAX_NODES];
extern const int g_8g_num_nodes3;

#ifdef __cplusplus
}
#endif

#endif
