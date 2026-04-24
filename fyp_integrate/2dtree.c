#include "utils.h"
/**
 * @brief �ָ��ַ����������Ų�֣����ڽ��������У�
 * @param str ���ָ���ַ���
 * @param delim �ָ������˴�Ϊ","��
 * @param res �ָ��Ľ�����飨����ǰ����ռ䣩
 * @return �ָ����ֶ���
 */
int split_str(char *str, const char *delim, char **res)
{
    int count = 0;
    char *token = strtok(str, delim);
    while (token != NULL && count < 7)
    { // ÿ�й̶�7���ֶ�
        res[count++] = token;
        token = strtok(NULL, delim);
    }
    return count;
}

/**
 * @brief ��txt�ļ����ؾ���������
 * @param filename �����ļ�·������"dtree_params.txt"��
 * @return 0=�ɹ���-1=�ļ���ʧ�ܣ�-2=��ȡ�ڵ���ʧ�ܣ�-3=��ȡ�ڵ����ʧ��
 */
int dtree_load_params(const char *filename)
{
    (void)filename;
    return -ENOTSUP;
}

/**
 * @brief ������Ԥ�⺯���������߼���
 * @param x �����������飨0-based����������Matlabѵ��ʱ��������һ�£�
 * @return Ԥ�����Ҷ�ڵ��node_class����-1=Ԥ��ʧ��
 */
int dtree_predict(const double *x)
{
    double features[6];
    standardize_features(x, features);
    if (dtree_num_nodes == 0)
    {
        fprintf(stderr, "����δ���ؾ�����������\n");
        return -1;
    }

    int current_node_id = 1; // ���ڵ�ID�̶�Ϊ1��Matlab����������
    while (1)
    {
        // �ڵ�IDת����������1-based �� 0-based��
        int idx = current_node_id - 1;
        if (idx < 0 || idx >= dtree_num_nodes)
        {
            fprintf(stderr, "���󣺽ڵ�ID %d ������Χ��\n", current_node_id);
            return -1;
        }
        const DTreeNode *node = &dtree_nodes[idx];

        // Ҷ�ڵ㣺ֱ�ӷ���Ԥ�����
        if (node->is_branch == 0)
        {
            return node->node_class;
        }

        // ��֧�ڵ㣺��֤����������Ч��
        int feat_idx = node->cut_pred_idx - 1; // 1-based �� 0-based
        if (feat_idx < 0)
        {
            fprintf(stderr, "���󣺽ڵ� %d ������������Ч��\n", current_node_id);
            return -1;
        }

        // �ָ�Ƚϣ�ʹ�� <= ����
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
