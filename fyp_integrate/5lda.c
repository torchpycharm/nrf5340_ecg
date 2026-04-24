#include "utils.h"
// �������棨��˹��Ԫ��������D��D����
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

    // ��˹��Ԫ
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
        } // ��������
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

    // ��ȡ�����
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
            inv[i][j] = aug[i][j + n];
        free(aug[i]);
    }
    free(aug);
    return 1;
}

// ��ȡLDA������Ԥ���������ƫ����
LDAParams loadLDAParams(const char *filename)
{
    LDAParams params;
    FILE *fid = fopen(filename, "r");
    if (!fid)
    {
        perror("�ļ���ʧ��");
        exit(1);
    }

    // ��ȡK��D
    fscanf(fid, "%d %d", &params.K2, &params.D);

    // �����ڴ�
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

    // ��ȡ����ǩ���������
    for (int i = 0; i < params.K2; i++)
        fscanf(fid, "%lf", &params.classNames[i]);
    for (int i = 0; i < params.K2; i++)
        fscanf(fid, "%lf", &params.prior[i]);

    // ��ȡ��ֵ����
    for (int i = 0; i < params.K2; i++)
    {
        for (int j = 0; j < params.D; j++)
        {
            fscanf(fid, "%lf", &params.mu[i][j]);
        }
    }

    // ��ȡЭ�����������
    for (int i = 0; i < params.D; i++)
    {
        for (int j = 0; j < params.D; j++)
        {
            fscanf(fid, "%lf", &sigma[i][j]);
        }
    }
    if (!matrixInverse(sigma, params.sigma_inv, params.D))
    {
        fprintf(stderr, "Э����������죬�޷�����\n");
        exit(1);
    }

    // Ԥ�����б���ƫ���-0.5*��_k^T��^{-1}��_k + ln(Prior_k)
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

    // �ͷ���ʱ�ڴ�
    for (int i = 0; i < params.D; i++)
        free(sigma[i]);
    free(sigma);
    fclose(fid);
    return params;
}

// �ͷ�LDA�����ڴ�
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

// �������
double vecDot(const double *a, const double *b, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += a[i] * b[i];
    return sum;
}

// ����������x: Dά��A: D��D��res: Dά��
void vecMatMul(const double *x, double **A, double *res, int D)
{
    for (int i = 0; i < D; i++)
    {
        res[i] = 0.0;
        for (int j = 0; j < D; j++)
            res[i] += x[j] * A[i][j];
    }
}

// LDAԤ�⺯��������6ά��������x������Ԥ�����
double ldaPredict(const LDAParams *params, const double *x)
{
    double features[6];
    standardize_features(x, features);
    double maxScore = -1e20;
    int bestClass = 0;

    // ����x^T��^{-1}
    double *x_sigma_inv = (double *)malloc(params->D * sizeof(double));
    vecMatMul(features, params->sigma_inv, x_sigma_inv, params->D);

    // ����ÿ�������б���ֵ
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
