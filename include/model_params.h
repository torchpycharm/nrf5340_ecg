/**
 * @file model_params.h
 * @brief 决策树模型参数定义
 * 
 * 该文件包含从MATLAB训练得到的决策树模型参数
 */

#ifndef MODEL_PARAMS_H
#define MODEL_PARAMS_H

#include <stdint.h>

#define N_FEATURES 6    // 特征数量: 3个峰度 + 3个偏度
#define N_NODES 43      // 决策树节点数量

/* 决策树结构参数 */
extern const int8_t  cut_feature[N_NODES];   // 每个节点的分裂特征索引
extern const float   cut_value[N_NODES];     // 每个节点的分裂阈值
extern const int16_t left_child[N_NODES];    // 左子节点索引
extern const int16_t right_child[N_NODES];   // 右子节点索引
extern const int8_t  is_branch[N_NODES];     // 是否为分支节点(1)或叶子节点(0)
extern const int8_t  node_class[N_NODES];    // 叶子节点的类别

/* 特征标准化参数 */
extern const float   mean_all[N_FEATURES];   // 训练集特征均值
extern const float   std_all[N_FEATURES];    // 训练集特征标准差

#endif /* MODEL_PARAMS_H */
