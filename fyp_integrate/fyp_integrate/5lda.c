#include "utils.h"
// 矩阵求逆（高斯消元法，处理D×D矩阵）
int matrixInverse(double **mat, double **inv, int n)
{
    double **aug = (double **)malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++)
    {
        aug[i] = (double *)malloc(2 * n * sizeof(double));
        for (int j = 0; j < n; j++)
        {
            aug[i][j] = mat[i][j];
            aug[i][j + n] = (i == j) ? 1.0 : 0.0;
        }
    }

    // 高斯消元
    for (int i = 0; i < n; i++)
    {
        int pivot = i;
        for (int j = i; j < n; j++)
        {
            if (fabs(aug[j][i]) > fabs(aug[pivot][i]))
                pivot = j;
        }
        if (fabs(aug[pivot][i]) < 1e-10)
        {
            free(aug);
            return 0;
        } // 矩阵奇异
        double *temp = aug[i];
        aug[i] = aug[pivot];
        aug[pivot] = temp;
        double div = aug[i][i];
        for (int j = i; j < 2 * n; j++)
            aug[i][j] /= div;
        for (int j = 0; j < n; j++)
        {
            if (j != i)
            {
                double factor = aug[j][i];
                for (int k = i; k < 2 * n; k++)
                    aug[j][k] -= factor * aug[i][k];
            }
        }
    }

    // 提取逆矩阵
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
            inv[i][j] = aug[i][j + n];
        free(aug[i]);
    }
    free(aug);
    return 1;
}

// 读取LDA参数并预计算逆矩阵、偏置项
LDAParams loadLDAParams(const char *filename)
{
    LDAParams params;
    FILE *fid = fopen(filename, "r");
    if (!fid)
    {
        perror("文件打开失败");
        exit(1);
    }

    // 读取K、D
    fscanf(fid, "%d %d", &params.K2, &params.D);

    // 分配内存
    params.classNames = (double *)malloc(params.K2 * sizeof(double));
    params.prior = (double *)malloc(params.K2 * sizeof(double));
    params.mu = (double **)malloc(params.K2 * sizeof(double *));
    for (int i = 0; i < params.K2; i++)
    {
        params.mu[i] = (double *)malloc(params.D * sizeof(double));
    }
    double **sigma = (double **)malloc(params.D * sizeof(double *));
    params.sigma_inv = (double **)malloc(params.D * sizeof(double *));
    for (int i = 0; i < params.D; i++)
    {
        sigma[i] = (double *)malloc(params.D * sizeof(double));
        params.sigma_inv[i] = (double *)malloc(params.D * sizeof(double));
    }
    params.bias = (double *)malloc(params.K2 * sizeof(double));

    // 读取类别标签、先验概率
    for (int i = 0; i < params.K2; i++)
        fscanf(fid, "%lf", &params.classNames[i]);
    for (int i = 0; i < params.K2; i++)
        fscanf(fid, "%lf", &params.prior[i]);

    // 读取均值矩阵
    for (int i = 0; i < params.K2; i++)
    {
        for (int j = 0; j < params.D; j++)
        {
            fscanf(fid, "%lf", &params.mu[i][j]);
        }
    }

    // 读取协方差矩阵并求逆
    for (int i = 0; i < params.D; i++)
    {
        for (int j = 0; j < params.D; j++)
        {
            fscanf(fid, "%lf", &sigma[i][j]);
        }
    }
    if (!matrixInverse(sigma, params.sigma_inv, params.D))
    {
        fprintf(stderr, "协方差矩阵奇异，无法求逆\n");
        exit(1);
    }

    // 预计算判别函数偏置项：-0.5*μ_k^TΣ^{-1}μ_k + ln(Prior_k)
    for (int k = 0; k < params.K2; k++)
    {
        double *temp = (double *)malloc(params.D * sizeof(double));
        for (int i = 0; i < params.D; i++)
        {
            temp[i] = 0.0;
            for (int j = 0; j < params.D; j++)
            {
                temp[i] += params.mu[k][j] * params.sigma_inv[i][j];
            }
        }
        double mu_term = 0.0;
        for (int i = 0; i < params.D; i++)
            mu_term += params.mu[k][i] * temp[i];
        params.bias[k] = -0.5 * mu_term + log(params.prior[k]);
        free(temp);
    }

    // 释放临时内存
    for (int i = 0; i < params.D; i++)
        free(sigma[i]);
    free(sigma);
    fclose(fid);
    return params;
}

// 释放LDA参数内存
void freeLDAParams(LDAParams *params)
{
    free(params->classNames);
    free(params->prior);
    for (int i = 0; i < params->K2; i++)
        free(params->mu[i]);
    free(params->mu);
    for (int i = 0; i < params->D; i++)
        free(params->sigma_inv[i]);
    free(params->sigma_inv);
    free(params->bias);
}

// 向量点积
double vecDot(double *a, double *b, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += a[i] * b[i];
    return sum;
}

// 向量×矩阵（x: D维，A: D×D，res: D维）
void vecMatMul(double *x, double **A, double *res, int D)
{
    for (int i = 0; i < D; i++)
    {
        res[i] = 0.0;
        for (int j = 0; j < D; j++)
            res[i] += x[j] * A[i][j];
    }
}

// LDA预测函数（输入6维特征向量x，返回预测类别）
double ldaPredict(LDAParams *params, double *x)
{
    double features[6];
    standardize_features((double *)x, features);
    double maxScore = -1e20;
    int bestClass = 0;

    // 计算x^TΣ^{-1}
    double *x_sigma_inv = (double *)malloc(params->D * sizeof(double));
    vecMatMul(features, params->sigma_inv, x_sigma_inv, params->D);

    // 计算每个类别的判别函数值
    for (int k = 0; k < params->K2; k++)
    {
        double score = vecDot(x_sigma_inv, params->mu[k], params->D) + params->bias[k];
        if (score > maxScore)
        {
            maxScore = score;
            bestClass = k;
        }
    }

    free(x_sigma_inv);
    return params->classNames[bestClass];
}
