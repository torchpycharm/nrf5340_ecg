#include "utils.h"
/**
 * @brief ��txt�ļ����ؾ���������
 * @param filename �����ļ�·������"dtree_params.txt"��
 * @return 0=�ɹ���-1=�ļ���ʧ�ܣ�-2=��ȡ�ڵ���ʧ�ܣ�-3=��ȡ�ڵ����ʧ��
 */
int dtree_load_params_choice(const char *filename, DTreeNode *out_nodes, int *out_num_nodes)
{
    if (out_nodes == NULL || out_num_nodes == NULL)
    {
        fprintf(stderr, "���������������Ϊ�գ�\n");
        return -4;
    }

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "���󣺴��ļ� %s ʧ�ܣ�ԭ��%s\n", filename, strerror(errno));
        return -1;
    }

    // ��һ������ȡ�ڵ���������һ�У�
    int num_nodes = 0;
    if (fscanf(fp, "%d\n", &num_nodes) != 1)
    { // \n�������з�
        fprintf(stderr, "���󣺶�ȡ�ڵ�����ʧ�ܣ�\n");
        fclose(fp);
        return -2;
    }
    if (num_nodes <= 0 || num_nodes > MAX_NODES)
    {
        fprintf(stderr, "���󣺽ڵ��� %d ������Χ��0~%d����\n", num_nodes, MAX_NODES);
        fclose(fp);
        return -2;
    }

    // �ڶ��������ж�ȡÿ���ڵ�Ĳ���
    char line[256];  // �洢ÿ������
    char *fields[8]; // �洢�ָ���8���ֶ�
    for (int i = 0; i < num_nodes; i++)
    {
        // ��ȡһ�У���������/ע���У��˴��򻯴�����
        if (fgets(line, sizeof(line), fp) == NULL)
        {
            fprintf(stderr, "���󣺶�ȡ�� %d ���ڵ����ʧ�ܣ�\n", i + 1);
            fclose(fp);
            return -3;
        }

        // �ָ��ַ����������Ų�֣�
        int field_cnt = split_str(line, SEP, fields);
        if (field_cnt != 7)
        {
            fprintf(stderr, "���󣺵� %d ���ڵ������ʽ������7���ֶΣ�ʵ��%d������\n", i + 1, field_cnt);
            fclose(fp);
            return -3;
        }

        // ת���ֶ�Ϊ��Ӧ���ͣ�����ṹ�壨�ڵ�ID-1��Ӧ����0-based������
        DTreeNode *node = &out_nodes[atoi(fields[0]) - 1];
        node->node_id = atoi(fields[0]);
        node->is_branch = atoi(fields[1]);
        node->cut_pred_idx = atoi(fields[2]);
        node->cut_point = atof(fields[3]);
        node->node_class = atoi(fields[4]);
        node->left_child = atoi(fields[5]);
        node->right_child = atoi(fields[6]);
    }

    fclose(fp);
    *out_num_nodes = num_nodes;
    printf("�ɹ����ؾ�����������%s\n", filename);
    printf("�ڵ�������%d\n", num_nodes);
    return 0;
}

/**
 * @brief ͨ�õľ�����Ԥ�⺯����֧�ִ������������
 * @param nodes �������ڵ����飨0-based �洢���ڵ�� node_id Ϊ 1-based��
 * @param num_nodes ��������Ч�ڵ�����
 * @param x ԭʼ����������δ��׼����
 * @return Ԥ�����Ҷ�ڵ�� node_class����-1=Ԥ��ʧ��
 */
int dtree_predict_choice(const DTreeNode *nodes, int num_nodes, const double *x)
{
    double features[FEAT_DIM];
    standardize_features(x, features);

    if (num_nodes == 0 || nodes == NULL)
    {
        fprintf(stderr, "����δ�ṩ������������\n");
        return -1;
    }

    int current_node_id = 1; // ���ڵ�ID�̶�Ϊ1��Matlab����������
    while (1)
    {
        int idx = current_node_id - 1; // 1-based -> 0-based
        if (idx < 0 || idx >= num_nodes)
        {
            fprintf(stderr, "���󣺽ڵ�ID %d ������Χ��num_nodes=%d����\n", current_node_id, num_nodes);
            return -1;
        }

        const DTreeNode *node = &nodes[idx];

        // Ҷ�ڵ㣺�������
        if (node->is_branch == 0)
        {
            return node->node_class;
        }

        int feat_idx = node->cut_pred_idx - 1; // 1-based -> 0-based
        if (feat_idx < 0 || feat_idx >= FEAT_DIM)
        {
            fprintf(stderr, "���󣺽ڵ� %d ������������Ч��%d����\n", current_node_id, node->cut_pred_idx);
            return -1;
        }

        if (features[feat_idx] <= node->cut_point)
        {
            current_node_id = node->left_child;
        }
        else
        {
            current_node_id = node->right_child;
        }
    }
}

int getMedian(int a, int b, int c)
{
    // ����1��a����λ����a��b��c֮�䣩
    if ((a >= b && a <= c) || (a <= b && a >= c))
    {
        return a;
    }
    // ����2��b����λ����b��a��c֮�䣩
    else if ((b >= a && b <= c) || (b <= a && b >= c))
    {
        return b;
    }
    // ����3��c����λ����ʣ�µ������
    else
    {
        return c;
    }
}