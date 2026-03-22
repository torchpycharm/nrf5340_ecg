#include "utils.h"
/**
 * @brief 从txt文件加载决策树参数
 * @param filename 参数文件路径（如"dtree_params.txt"）
 * @return 0=成功，-1=文件打开失败，-2=读取节点数失败，-3=读取节点参数失败
 */
int dtree_load_params_choice(const char *filename, DTreeNode *out_nodes, int *out_num_nodes)
{
    if (out_nodes == NULL || out_num_nodes == NULL)
    {
        fprintf(stderr, "错误：输出参数不能为空！\n");
        return -4;
    }

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "错误：打开文件 %s 失败！原因：%s\n", filename, strerror(errno));
        return -1;
    }

    // 第一步：读取节点总数（第一行）
    int num_nodes = 0;
    if (fscanf(fp, "%d\n", &num_nodes) != 1)
    { // \n跳过换行符
        fprintf(stderr, "错误：读取节点总数失败！\n");
        fclose(fp);
        return -2;
    }
    if (num_nodes <= 0 || num_nodes > MAX_NODES)
    {
        fprintf(stderr, "错误：节点数 %d 超出范围（0~%d）！\n", num_nodes, MAX_NODES);
        fclose(fp);
        return -2;
    }

    // 第二步：逐行读取每个节点的参数
    char line[256];  // 存储每行内容
    char *fields[8]; // 存储分割后的8个字段
    for (int i = 0; i < num_nodes; i++)
    {
        // 读取一行（跳过空行/注释行，此处简化处理）
        if (fgets(line, sizeof(line), fp) == NULL)
        {
            fprintf(stderr, "错误：读取第 %d 个节点参数失败！\n", i + 1);
            fclose(fp);
            return -3;
        }

        // 分割字符串（按逗号拆分）
        int field_cnt = split_str(line, SEP, fields);
        if (field_cnt != 7)
        {
            fprintf(stderr, "错误：第 %d 个节点参数格式错误（需7个字段，实际%d个）！\n", i + 1, field_cnt);
            fclose(fp);
            return -3;
        }

        // 转换字段为对应类型，存入结构体（节点ID-1对应数组0-based索引）
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
    printf("成功加载决策树参数：%s\n", filename);
    printf("节点总数：%d\n", num_nodes);
    return 0;
}

/**
 * @brief 通用的决策树预测函数，支持传入任意参数集
 * @param nodes 决策树节点数组（0-based 存储，节点的 node_id 为 1-based）
 * @param num_nodes 数组中有效节点数量
 * @param x 原始输入特征（未标准化）
 * @return 预测类别（叶节点的 node_class），-1=预测失败
 */
int dtree_predict_choice(const DTreeNode *nodes, int num_nodes, const double *x)
{
    double features[FEAT_DIM];
    // 需要去掉 constness 才能复用现有标准化函数
    standardize_features((double *)x, features);

    if (num_nodes == 0 || nodes == NULL)
    {
        fprintf(stderr, "错误：未提供决策树参数！\n");
        return -1;
    }

    int current_node_id = 1; // 根节点ID固定为1（Matlab决策树规则）
    while (1)
    {
        int idx = current_node_id - 1; // 1-based -> 0-based
        if (idx < 0 || idx >= num_nodes)
        {
            fprintf(stderr, "错误：节点ID %d 超出范围（num_nodes=%d）！\n", current_node_id, num_nodes);
            return -1;
        }

        const DTreeNode *node = &nodes[idx];

        // 叶节点：返回类别
        if (node->is_branch == 0)
        {
            return node->node_class;
        }

        int feat_idx = node->cut_pred_idx - 1; // 1-based -> 0-based
        if (feat_idx < 0 || feat_idx >= FEAT_DIM)
        {
            fprintf(stderr, "错误：节点 %d 的特征索引无效（%d）！\n", current_node_id, node->cut_pred_idx);
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
    // 条件1：a是中位数（a在b和c之间）
    if ((a >= b && a <= c) || (a <= b && a >= c))
    {
        return a;
    }
    // 条件2：b是中位数（b在a和c之间）
    else if ((b >= a && b <= c) || (b <= a && b >= c))
    {
        return b;
    }
    // 条件3：c是中位数（剩下的情况）
    else
    {
        return c;
    }
}