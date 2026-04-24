#include "utils.h"
// ��ȡ�����ļ�
NBParams loadNBParams(const char *filename)
{
    NBParams params;
    FILE *fid = fopen(filename, "r");
    if (!fid)
    {
        perror("�ļ���ʧ��");
        exit(EXIT_FAILURE);
    }

    // ��ȡ�������������
    fscanf(fid, "%d %d", &params.K1, &params.D);

    // �����ڴ�
    params.classNames = (double *)malloc(params.K1 * sizeof(double));
    params.prior = (double *)malloc(params.K1 * sizeof(double));
    params.mu = (double **)malloc(params.K1 * sizeof(double *));
    params.sigma = (double **)malloc(params.K1 * sizeof(double *));
    for (int i = 0; i < params.K1; i++)
    {
        params.mu[i] = (double *)malloc(params.D * sizeof(double));
        params.sigma[i] = (double *)malloc(params.D * sizeof(double));
    }

    // ��ȡ����ǩ
    for (int i = 0; i < params.K1; i++)
    {
        fscanf(fid, "%lf", &params.classNames[i]);
    }
    // ��ȡ�������
    for (int i = 0; i < params.K1; i++)
    {
        fscanf(fid, "%lf", &params.prior[i]);
    }
    // ��ȡ��ֵ����
    for (int i = 0; i < params.K1; i++)
    {
        for (int j = 0; j < params.D; j++)
        {
            fscanf(fid, "%lf", &params.mu[i][j]);
        }
    }
    // ��ȡ�������
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

// �ͷŲ����ڴ�
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

// ��˹�����ܶȺ�����x:����ֵ��mu:��ֵ��sigma:���
double gaussianPdf(double x, double mu, double sigma)
{
    const double k_pi = 3.14159265358979323846;

    if (sigma <= 1e-6)
    { // ���ⷽ��Ϊ0����ֵ�ȶ��ԣ�
        return (x == mu) ? 1.0 : 0.0;
    }
    double coeff = 1.0 / (sqrt(2.0 * k_pi) * sigma);
    double exponent = -pow(x - mu, 2) / (2 * sigma * sigma);
    return coeff * exp(exponent);
}

// ���ر�Ҷ˹Ԥ�⺯��������6ά��������x������Ԥ�����
double nbPredict(const NBParams *params, const double *x)
{
    double features[6];
    standardize_features(x, features);
    double maxPosterior = -1.0;
    int bestClassIdx = 0;

    // ����ÿ����𣬼���������
    for (int k = 0; k < params->K1; k++)
    {
        double posterior = params->prior[k]; // �������
        // ����ÿ���������������ʣ���˹�ܶȣ�
        for (int d = 0; d < params->D; d++)
        {
            posterior *= gaussianPdf(features[d], params->mu[k][d], params->sigma[k][d]);
        }
        // ������������ʶ�Ӧ�����
        if (posterior > maxPosterior)
        {
            maxPosterior = posterior;
            bestClassIdx = k;
        }
    }
    return params->classNames[bestClassIdx];
}
