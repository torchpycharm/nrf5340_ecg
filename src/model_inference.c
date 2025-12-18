/**
 * @file model_inference.c
 * @brief 决策树推理模块实现
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "model_inference.h"
#include "model_params.h"

LOG_MODULE_REGISTER(model_inference, LOG_LEVEL_INF);

/**
 * @brief 标准化特征向量
 * 
 * 使用Z-score标准化: x_norm = (x - mean) / std
 */
void standardize_features(const feature_vector_t *features_raw, 
                         float *features_normalized)
{
    const float *raw_array = (const float *)features_raw;
    
    for (int i = 0; i < N_FEATURES; i++) {
        if (std_all[i] != 0.0f) {
            features_normalized[i] = (raw_array[i] - mean_all[i]) / std_all[i];
        } else {
            features_normalized[i] = raw_array[i];
        }
    }
    
    LOG_DBG("Standardized features:");
    for (int i = 0; i < N_FEATURES; i++) {
        LOG_DBG("  f[%d]: %.4f -> %.4f", i, raw_array[i], features_normalized[i]);
    }
}

/**
 * @brief 决策树推理函数
 */
int tree_predict(const feature_vector_t *features)
{
    if (!features) {
        return -1;
    }
    
    /* 标准化特征 */
    float features_norm[N_FEATURES];
    standardize_features(features, features_norm);
    
    /* 从根节点开始遍历决策树 */
    int node = 0;
    int depth = 0;
    
    while (1) {
        /* 检查是否为叶子节点 */
        if (!is_branch[node]) {
            int result = node_class[node];
            LOG_INF("Reached leaf node %d at depth %d, class = %d", 
                    node, depth, result);
            return result;
        }
        
        /* 获取分裂特征索引 */
        int feature_index = cut_feature[node];
        
        /* 安全检查 */
        if (feature_index < 0 || feature_index >= N_FEATURES) {
            LOG_WRN("Invalid feature index %d at node %d, using class %d", 
                    feature_index, node, node_class[node]);
            return node_class[node];
        }
        
        /* 获取分裂阈值 */
        float split_value = cut_value[node];
        
        /* 决策：左子树 or 右子树 */
        int next_node;
        if (features_norm[feature_index] < split_value) {
            next_node = left_child[node];
            LOG_DBG("Node %d: f[%d]=%.4f < %.4f, go left to %d", 
                    node, feature_index, features_norm[feature_index], 
                    split_value, next_node);
        } else {
            next_node = right_child[node];
            LOG_DBG("Node %d: f[%d]=%.4f >= %.4f, go right to %d", 
                    node, feature_index, features_norm[feature_index], 
                    split_value, next_node);
        }
        
        /* 安全检查 */
        if (next_node < 0 || next_node >= N_NODES) {
            LOG_WRN("Invalid child node %d, using class %d", 
                    next_node, node_class[node]);
            return node_class[node];
        }
        
        node = next_node;
        depth++;
        
        /* 防止无限循环 */
        if (depth > N_NODES) {
            LOG_ERR("Tree traversal too deep, possible loop");
            return 0;
        }
    }
}
