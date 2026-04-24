// utils.h
#ifndef UTILS_H // 头文件保护
#define UTILS_H // 定义UTILS_H

// 引用库
#include <stdio.h>
#define _USE_MATH_DEFINES // 必须放在math.h之前
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h> // 用于tolower函数

// 公用常量
#define FEAT_DIM 6 // 特征维度
// 公共函数
void standardize_features(const double *x_in, double *x_out);

// 1knn=============================================================
#define TRAIN_NUM 462 // 训练样本数
#define K 1           // k值
// 全局变量存储训练数据
extern const double train_X[TRAIN_NUM][FEAT_DIM]; // 训练特征
extern const int train_Y[TRAIN_NUM];              // 训练标签
// 读取训练特征文件
int load_train_X(const char *filename);
// 读取训练标签文件
int load_train_Y(const char *filename);
// 计算测试样本与单个训练样本的欧几里得距离的平方
double calc_euclidean_sq(const double test_feat[FEAT_DIM], const double train_feat[FEAT_DIM]);
// 1-NN预测函数
int knn_predict(const double test_feat[FEAT_DIM]);

// 2dtree===========================================================
//  配置：根据实际决策树节点数调整
#define MAX_NODES 200
// 分割字符串的分隔符
#define SEP ","
// 决策树节点结构体
typedef struct
{
    int node_id;      // 节点ID（1-based）
    int is_branch;    // 1=分支节点，0=叶节点
    int cut_pred_idx; // 特征索引（1-based，叶节点为0）
    double cut_point; // 分割阈值（叶节点为0）
    int node_class;   // 叶节点类别（分支节点为0）
    int left_child;   // 左子节点ID（1-based）
    int right_child;  // 右子节点ID（1-based）
} DTreeNode;
// 全局存储决策树参数
extern const DTreeNode dtree_nodes[MAX_NODES];
extern const int dtree_num_nodes;
// 分割字符串（按逗号拆分，用于解析参数行）
int split_str(char *str, const char *delim, char **res);
// 从txt文件加载决策树参数
int dtree_load_params(const char *filename);
// 决策树预测函数
int dtree_predict(const double *x);

// 3svm===========================================================
// 存储SVM模型参数的结构体
typedef struct
{
    int sv_num;           // 支持向量数量
    int feat_dim;         // 特征维度（这里是6）
    const double **support_vec; // 支持向量矩阵（sv_num × feat_dim）
    const double *alpha;        // Alpha系数数组（长度sv_num）
    const double *label;        // 支持向量标签数组（长度sv_num）
    double bias;          // 偏置项
    double gamma;         // RBF核宽度
} SVMModel;
// 释放SVM模型占用的动态内存
void free_svm_model(SVMModel *model);
// 从指定文件加载SVM模型参数
SVMModel *load_svm_model(const char *filename);
// 计算RBF核函数值（静态函数，仅当前文件可见）
double rbf_kernel(const double x[], const double sv[], int feat_dim, double gamma);
// SVM预测函数（基于RBF核的SVM分类）
int svm_predict(const SVMModel *model, const double x[]);

// 4Bayes===========================================================
// 结构体
typedef struct
{
    int K1;             // 类别数
    int D;              // 特征数
    double *classNames; // 类别标签 [0,1]
    double *prior;      // 先验概率
    double **mu;        // 均值矩阵（K×D）
    double **sigma;     // 方差矩阵（K×D）
} NBParams;
// 读取朴素贝叶斯参数文件
NBParams loadNBParams(const char *filename);
// 释放朴素贝叶斯参数内存
void freeNBParams(NBParams *params);
// 高斯概率密度函数计算
double gaussianPdf(double x, double mu, double sigma);
// 朴素贝叶斯预测（输入特征返回预测类别）
double nbPredict(const NBParams *params, const double *x);

// 5lda===========================================================
// LDA参数结构体
typedef struct
{
    int K2;             // 类别数
    int D;              // 特征数
    double *classNames; // 类别标签 [0,1]
    double *prior;      // 先验概率
    double **mu;        // 类别均值（K×D）
    double **sigma_inv; // 协方差矩阵的逆（D×D）
    double *bias;       // 每个类别的判别函数偏置项（K维）
} LDAParams;
// 矩阵求逆
int matrixInverse(double **mat, double **inv, int n);
// 读取LDA参数并预计算协方差逆矩阵、判别偏置项
LDAParams loadLDAParams(const char *filename);
// 释放LDA参数占用的动态内存
void freeLDAParams(LDAParams *params);
// 计算两个n维向量的点积
double vecDot(const double *a, const double *b, int n);
// 向量(x,D维) × 矩阵(A,D×D)，结果存入res(D维)
void vecMatMul(const double *x, double **A, double *res, int D);
// LDA预测
double ldaPredict(const LDAParams *params, const double *x);

// 7e_dtree=========================================================
// 加载决策树参数到指定节点数组，返回状态码
int dtree_load_params_choice(const char *filename, DTreeNode *out_nodes, int *out_num_nodes);
// 通用决策树预测
int dtree_predict_choice(const DTreeNode *nodes, int num_nodes, const double *x);
// 计算三个整数的中位数并返回
int getMedian(int a, int b, int c);

// 8g_dtree=========================================================
// 梯度提升集成预测
int gboosting_predict(double pred12, double pred13, double pred14);

// 11mix1=========================================================
// 单样本多模型结果融合，返回融合后的二分类结果
int fuseSingleSample1(int ped_class2_val, int ped_class4_val, int ped_class5_val);

// 12mix2=========================================================
// 单样本双模型结果融合，返回二分类结果
int fuseSingleSample2(int ped_class4_val, int ped_class5_val);

// 13mix3=========================================================
// 多模型投票融合预测，6模型均值决策，平局时子模型重判，返回二分类结果
int modelVoteFusion(int ped_class1_val, int ped_class2_val,
                    int ped_class3_val, int ped_class4_val,
                    int ped_class5_val, int ped_class7_val);

#endif // 结束头文件保护