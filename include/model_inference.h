/**
 * @file model_inference.h
 * @brief 决策树推理模块
 * 
 * 该模块使用训练好的决策树模型对提取的特征进行分类
 */

#ifndef MODEL_INFERENCE_H
#define MODEL_INFERENCE_H

#include <stdint.h>
#include "online_moments.h"

/**
 * @brief 决策树推理函数
 * @param features 输入特征向量(6维)
 * @return 分类结果: 0 或 1
 * 
 * 该函数会自动进行特征标准化，然后通过决策树进行推理
 */
int tree_predict(const feature_vector_t *features);

/**
 * @brief 标准化特征向量
 * @param features_raw 原始特征向量
 * @param features_normalized 标准化后的特征向量
 * 
 * 使用训练集的均值和标准差进行Z-score标准化
 */
void standardize_features(const feature_vector_t *features_raw, 
                         float *features_normalized);

#endif /* MODEL_INFERENCE_H */
