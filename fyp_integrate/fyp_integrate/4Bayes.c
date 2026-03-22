#include "utils.h"
// 读取参数文件
NBParams loadNBParams(const char *filename)
{
    NBParams params;
    FILE *fid = fopen(filename, "r");
    if (!fid)
    {
        perror("文件打开失败");
        exit(EXIT_FAILURE);
    }

    // 读取类别数、特征数
    fscanf(fid, "%d %d", &params.K1, &params.D);

    // 分配内存
    params.classNames = (double *)malloc(params.K1 * sizeof(double));
    params.prior = (double *)malloc(params.K1 * sizeof(double));
    params.mu = (double **)malloc(params.K1 * sizeof(double *));
    params.sigma = (double **)malloc(params.K1 * sizeof(double *));
    for (int i = 0; i < params.K1; i++)
    {
        params.mu[i] = (double *)malloc(params.D * sizeof(double));
        params.sigma[i] = (double *)malloc(params.D * sizeof(double));
    }

    // 读取类别标签
    for (int i = 0; i < params.K1; i++)
    {
        fscanf(fid, "%lf", &params.classNames[i]);
    }
    // 读取先验概率
    for (int i = 0; i < params.K1; i++)
    {
        fscanf(fid, "%lf", &params.prior[i]);
    }
    // 读取均值矩阵
    for (int i = 0; i < params.K1; i++)
    {
        for (int j = 0; j < params.D; j++)
        {
            fscanf(fid, "%lf", &params.mu[i][j]);
        }
    }
    // 读取方差矩阵
    for (int i = 0; i < params.K1; i++)
    {
        for (int j = 0; j < params.D; j++)
        {
            fscanf(fid, "%lf", &params.sigma[i][j]);
        }
    }

    fclose(fid);
    return params;
}

// 释放参数内存
void freeNBParams(NBParams *params)
{
    free(params->classNames);
    free(params->prior);
    for (int i = 0; i < params->K1; i++)
    {
        free(params->mu[i]);
        free(params->sigma[i]);
    }
    free(params->mu);
    free(params->sigma);
}

// 高斯概率密度函数（x:特征值，mu:均值，sigma:方差）
double gaussianPdf(double x, double mu, double sigma)
{
    if (sigma <= 1e-6)
    { // 避免方差为0（数值稳定性）
        return (x == mu) ? 1.0 : 0.0;
    }
    double coeff = 1.0 / (sqrt(2 * M_PI) * sigma);
    double exponent = -pow(x - mu, 2) / (2 * sigma * sigma);
    return coeff * exp(exponent);
}

// 朴素贝叶斯预测函数（输入6维特征向量x，返回预测类别）
double nbPredict(NBParams *params, double *x)
{
    double features[6];
    standardize_features((double *)x, features);
    double maxPosterior = -1.0;
    int bestClassIdx = 0;

    // 遍历每个类别，计算后验概率
    for (int k = 0; k < params->K1; k++)
    {
        double posterior = params->prior[k]; // 先验概率
        // 乘以每个特征的条件概率（高斯密度）
        for (int d = 0; d < params->D; d++)
        {
            posterior *= gaussianPdf(features[d], params->mu[k][d], params->sigma[k][d]);
        }
        // 更新最大后验概率对应的类别
        if (posterior > maxPosterior)
        {
            maxPosterior = posterior;
            bestClassIdx = k;
        }
    }
    return params->classNames[bestClassIdx];
}
