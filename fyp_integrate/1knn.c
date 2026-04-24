#include "utils.h"

// ��ȡѵ�������ļ�
int load_train_X(const char *filename)
{
    (void)filename;
    return -ENOTSUP;
}

// ��ȡѵ����ǩ�ļ�
int load_train_Y(const char *filename)
{
    (void)filename;
    return -ENOTSUP;
}

// ������������뵥��ѵ��������ŷ����þ����ƽ����ʡ�Կ����ţ���Ӱ���С�Ƚϣ�
double calc_euclidean_sq(const double test_feat[FEAT_DIM], const double train_feat[FEAT_DIM])
{
    double dist_sq = 0.0;
    for (int i = 0; i < FEAT_DIM; i++)
    {
        double diff = test_feat[i] - train_feat[i];
        dist_sq += diff * diff;
    }
    return dist_sq;
}

// 1-NNԤ�⺯��
int knn_predict(const double test_feat[FEAT_DIM])
{
    double features[6];
    standardize_features(test_feat, features);
    double min_dist_sq = DBL_MAX; // ��ʼ����С����Ϊ����ֵ
    int pred_label = -1;          // ��ʼ��Ԥ���ǩ

    // �������ѵ���������������
    for (int i = 0; i < TRAIN_NUM; i++)
    {
        double dist_sq = calc_euclidean_sq(features, train_X[i]);
        // ������С����Ͷ�Ӧ�ı�ǩ
        if (dist_sq < min_dist_sq)
        {
            min_dist_sq = dist_sq;
            pred_label = train_Y[i];
        }
    }
    return pred_label;
}