#include "utils.h"

// 释放模型内存
void free_svm_model(SVMModel *model)
{
    if (model == NULL)
        return;
    // 释放支持向量矩阵
    if (model->support_vec != NULL)
    {
        for (int i = 0; i < model->sv_num; i++)
        {
            free((void *)model->support_vec[i]);
        }
        free((void *)model->support_vec);
    }
    // 释放Alpha数组
    free((void *)model->alpha);
    // 释放label数组
    free((void *)model->label);
    free(model);
}

// 从文件加载SVM模型
SVMModel *load_svm_model(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("打开模型文件失败");
        return NULL;
    }

    // 分配模型结构体
    SVMModel *model = (SVMModel *)malloc(sizeof(SVMModel));
    if (model == NULL)
    {
        fclose(fp);
        perror("分配模型内存失败");
        return NULL;
    }

    model->sv_num = 0;
    model->feat_dim = 0;
    model->support_vec = NULL;
    model->alpha = NULL;
    model->label = NULL;
    model->bias = 0.0;
    model->gamma = 0.0;

    // 1. 读取支持向量维度
    if (fscanf(fp, "%d %d", &model->sv_num, &model->feat_dim) != 2)
    {
        free(model);
        fclose(fp);
        fprintf(stderr, "模型文件格式错误（支持向量维度）\n");
        return NULL;
    }

    // 2. 分配并读取支持向量
    model->support_vec = (const double **)malloc(model->sv_num * sizeof(const double *));
    if (model->support_vec == NULL)
    {
        free(model);
        fclose(fp);
        perror("分配支持向量内存失败");
        return NULL;
    }
    for (int i = 0; i < model->sv_num; i++)
    {
        double *row = (double *)malloc(model->feat_dim * sizeof(double));
        if (row == NULL)
        {
            // 释放已分配的内存
            for (int j = 0; j < i; j++)
                free((void *)model->support_vec[j]);
            free((void *)model->support_vec);
            free(model);
            fclose(fp);
            perror("分配支持向量行内存失败");
            return NULL;
        }
        model->support_vec[i] = row;

        // 读取一行支持向量
        for (int j = 0; j < model->feat_dim; j++)
        {
            if (fscanf(fp, "%lf", &row[j]) != 1)
            {
                free_svm_model(model);
                fclose(fp);
                fprintf(stderr, "模型文件格式错误（支持向量数据）\n");
                return NULL;
            }
        }
    }

    // 3. 读取Alpha长度（校验是否等于支持向量数）
    int alpha_len;
    if (fscanf(fp, "%d", &alpha_len) != 1 || alpha_len != model->sv_num)
    {
        free_svm_model(model);
        fclose(fp);
        fprintf(stderr, "Alpha长度不匹配支持向量数\n");
        return NULL;
    }

    // 4. 分配并读取Alpha
    double *alpha = (double *)malloc(alpha_len * sizeof(double));
    if (alpha == NULL)
    {
        free_svm_model(model);
        fclose(fp);
        perror("分配Alpha内存失败");
        return NULL;
    }
    for (int i = 0; i < alpha_len; i++)
    {
        if (fscanf(fp, "%lf", &alpha[i]) != 1)
        {
            free(alpha);
            free_svm_model(model);
            fclose(fp);
            fprintf(stderr, "模型文件格式错误（Alpha数据）\n");
            return NULL;
        }
    }
    model->alpha = alpha;

    // 4.1 读取label长度（校验是否等于支持向量数）
    int label_len;
    if (fscanf(fp, "%d", &label_len) != 1 || label_len != model->sv_num)
    {
        free_svm_model(model);
        fclose(fp);
        fprintf(stderr, "label长度不匹配支持向量数\n");
        return NULL;
    }

    // 4.2 分配并读取label
    double *label = (double *)malloc(label_len * sizeof(double));
    if (label == NULL)
    {
        free_svm_model(model);
        fclose(fp);
        perror("分配label内存失败");
        return NULL;
    }
    for (int i = 0; i < label_len; i++)
    {
        if (fscanf(fp, "%lf", &label[i]) != 1)
        {
            free(label);
            free_svm_model(model);
            fclose(fp);
            fprintf(stderr, "模型文件格式错误（label数据）\n");
            return NULL;
        }
    }
    model->label = label;

    // 5. 读取Bias
    if (fscanf(fp, "%lf", &model->bias) != 1)
    {
        free_svm_model(model);
        fclose(fp);
        fprintf(stderr, "模型文件格式错误（Bias）\n");
        return NULL;
    }

    // 步骤6：读取gamma（替代原Sigma）
    if (fscanf(fp, "%lf", &model->gamma) != 1)
    {
        free_svm_model(model);
        fclose(fp);
        fprintf(stderr, "模型文件格式错误（gamma）\n");
        return NULL;
    }

    fclose(fp);
    return model;
}

// 修正RBF核：K(x, sv_i) = exp(-gamma * ||x - sv_i||?)
double rbf_kernel(const double x[], const double sv[], int feat_dim, double gamma)
{
    double dist_sq = 0.0;
    for (int j = 0; j < feat_dim; j++)
    {
        double diff = x[j] - sv[j];
        dist_sq += diff * diff;
    }
    return exp(-gamma * dist_sq);
}

// SVM预测函数：输入6维特征x，返回预测类别（0或1）
int svm_predict(const SVMModel *model, const double x[])
{

    double features[6];
    standardize_features(x, features);
double decision_val = 0.0;
    for (int i = 0; i < model->sv_num; i++)
    {
        // 传入gamma而非sigma
        double kernel_val = rbf_kernel(features, model->support_vec[i], model->feat_dim, model->gamma);
        decision_val += model->alpha[i] * model->label[i] * kernel_val;
    }
    decision_val += model->bias;
    return (decision_val >= 0) ? 1 : 0;
}
