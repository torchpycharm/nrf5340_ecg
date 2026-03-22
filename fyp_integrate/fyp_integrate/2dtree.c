#include "utils.h"
/**
 * @brief 分割字符串（按逗号拆分，用于解析参数行）
 * @param str 待分割的字符串
 * @param delim 分隔符（此处为","）
 * @param res 分割后的结果数组（需提前分配空间）
 * @return 分割后的字段数
 */
int split_str(char *str, const char *delim, char **res)
{
    int count = 0;
    char *token = strtok(str, delim);
    while (token != NULL && count < 7)
    { // 每行固定7个字段
        res[count++] = token;
        token = strtok(NULL, delim);
    }
    return count;
}

/**
 * @brief 从txt文件加载决策树参数
 * @param filename 参数文件路径（如"dtree_params.txt"）
 * @return 0=成功，-1=文件打开失败，-2=读取节点数失败，-3=读取节点参数失败
 */
int dtree_load_params(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "错误：打开文件 %s 失败！原因：%s\n", filename, strerror(errno));
        return -1;
    }

    // 第一步：读取节点总数（第一行）
    if (fscanf(fp, "%d\n", &dtree_num_nodes) != 1)
    { // \n跳过换行符
        fprintf(stderr, "错误：读取节点总数失败！\n");
        fclose(fp);
        return -2;
    }
    if (dtree_num_nodes <= 0 || dtree_num_nodes > MAX_NODES)
    {
        fprintf(stderr, "错误：节点数 %d 超出范围（0~%d）！\n", dtree_num_nodes, MAX_NODES);
        fclose(fp);
        return -2;
    }

    // 第二步：逐行读取每个节点的参数
    char line[256];  // 存储每行内容
    char *fields[8]; // 存储分割后的8个字段
    for (int i = 0; i < dtree_num_nodes; i++)
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
        DTreeNode *node = &dtree_nodes[atoi(fields[0]) - 1];
        node->node_id = atoi(fields[0]);
        node->is_branch = atoi(fields[1]);
        node->cut_pred_idx = atoi(fields[2]);
        node->cut_point = atof(fields[3]);
        node->node_class = atoi(fields[4]);
        node->left_child = atoi(fields[5]);
        node->right_child = atoi(fields[6]);
    }

    fclose(fp);
    printf("成功加载决策树参数！\n");
    printf("节点总数：%d\n", dtree_num_nodes);
    return 0;
}

/**
 * @brief 决策树预测函数（核心逻辑）
 * @param x 输入特征数组（0-based，长度需与Matlab训练时的特征数一致）
 * @return 预测类别（叶节点的node_class），-1=预测失败
 */
int dtree_predict(const double *x)
{
    double features[6];
    standardize_features((double *)x, features);
    if (dtree_num_nodes == 0)
    {
        fprintf(stderr, "错误：未加载决策树参数！\n");
        return -1;
    }

    int current_node_id = 1; // 根节点ID固定为1（Matlab决策树规则）
    while (1)
    {
        // 节点ID转数组索引（1-based → 0-based）
        int idx = current_node_id - 1;
        if (idx < 0 || idx >= dtree_num_nodes)
        {
            fprintf(stderr, "错误：节点ID %d 超出范围！\n", current_node_id);
            return -1;
        }
        DTreeNode *node = &dtree_nodes[idx];

        // 叶节点：直接返回预测类别
        if (node->is_branch == 0)
        {
            return node->node_class;
        }

        // 分支节点：验证特征索引有效性
        int feat_idx = node->cut_pred_idx - 1; // 1-based → 0-based
        if (feat_idx < 0)
        {
            fprintf(stderr, "错误：节点 %d 的特征索引无效！\n", current_node_id);
            return -1;
        }

        // 分割比较：使用 <= 规则
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
