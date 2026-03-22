#include "utils.h"
// 全局变量
// 1knn
double train_X[TRAIN_NUM][FEAT_DIM]; // 训练特征
int train_Y[TRAIN_NUM];              // 训练标签
// 2dtree
DTreeNode dtree_nodes[MAX_NODES];
int dtree_num_nodes = 0;

// 定义算法结构体：存储编号和名称，便于关联显示
typedef struct
{
    int id;
    const char *name;
} Algorithm;

// 定义需要遍历的算法列表（包含编号和对应的名称）
const Algorithm ALGORITHMS[] = {
    {1, "KNN"},
    {2, "决策树（基础版）"},
    {3, "SVM"},
    {4, "朴素贝叶斯"},
    {5, "LDA（线性判别分析）"},
    {7, "集成决策树（中位数融合）"},
    {8, "梯度提升决策树"},
    {11, "模型融合1（决策树+贝叶斯+LDA）"},
    {12, "模型融合2（贝叶斯+LDA）"},
    {13, "模型融合3（6模型投票）"}};
const int ALGO_COUNT = sizeof(ALGORITHMS) / sizeof(ALGORITHMS[0]);

// 计算并打印所有评估指标
void print_all_metrics(int tp, int tn, int fp, int fn, int total)
{
    // 避免除0
    float sensitivity = (tp + fn > 0) ? (float)tp / (tp + fn) : 0.0f;
    float specificity = (tn + fp > 0) ? (float)tn / (tn + fp) : 0.0f;
    float precision = (tp + fp > 0) ? (float)tp / (tp + fp) : 0.0f;
    float f1 = (2 * tp + fp + fn > 0) ? (float)(2 * tp) / (2 * tp + fp + fn) : 0.0f;
    float accuracy = (total > 0) ? (float)(tp + tn) / total : 0.0f;

    printf("\n==================== 预测结果统计 ====================\n");
    printf("混淆矩阵:\n");
    printf("          预测正例   预测负例\n");
    printf("真实正例   %-8d %-8d\n", tp, fn);
    printf("真实负例   %-8d %-8d\n", fp, tn);
    printf("------------------------------------------------------\n");
    printf("总有效样本数: %d\n", total);
    printf("准确率(Accuracy): %.4f\n", accuracy);
    printf("灵敏度(Sensitivity): %.4f\n", sensitivity);
    printf("特异度(Specificity): %.4f\n", specificity);
    printf("精确率(Precision): %.4f\n", precision);
    printf("F1-score: %.4f\n", f1);
    printf("======================================================\n");
}

int main()
{
    printf("=============== 开始自动执行所有算法 ===============\n");

    // 自动遍历所有算法（无互动、无暂停）
    for (int algo_idx = 0; algo_idx < ALGO_COUNT; algo_idx++)
    {
        int user_choice = ALGORITHMS[algo_idx].id;
        const char *algo_name = ALGORITHMS[algo_idx].name;

        printf("\n==========================================================\n");
        printf("正在执行算法 [%d] - %s ...\n", user_choice, algo_name);
        printf("==========================================================\n");

        // ========== 声明并初始化本次循环的模型变量 ==========
        SVMModel *svm_model = NULL;
        NBParams nb_params = {0};
        LDAParams lda_params = {0};
        DTreeNode tree7_1[MAX_NODES] = {0}, tree7_2[MAX_NODES] = {0}, tree7_3[MAX_NODES] = {0};
        int n7_1 = 0, n7_2 = 0, n7_3 = 0;
        DTreeNode tree8_1[MAX_NODES] = {0}, tree8_2[MAX_NODES] = {0}, tree8_3[MAX_NODES] = {0};
        int n8_1 = 0, n8_2 = 0, n8_3 = 0;
        int load_success = 1; // 模型加载成功标记

        // ========== 根据当前算法加载对应模型参数 ==========
        int load_ret = 0;
        switch (user_choice)
        {
        case 1: // KNN
            if (load_train_X("1knn_train_X.txt") != 0 || load_train_Y("1knn_train_Y.txt") != 0)
            {
                fprintf(stderr, "加载KNN训练数据失败！\n");
                load_success = 0;
            }
            break;

        case 2: // 基础决策树
            load_ret = dtree_load_params("2dtree_params.txt");
            if (load_ret != 0)
            {
                fprintf(stderr, "加载决策树参数失败！错误码：%d\n", load_ret);
                load_success = 0;
            }
            break;

        case 3: // SVM
            svm_model = load_svm_model("3svm_params.txt");
            if (svm_model == NULL)
            {
                fprintf(stderr, "加载SVM模型失败！\n");
                load_success = 0;
            }
            break;

        case 4: // 朴素贝叶斯
            nb_params = loadNBParams("4Bayes_params.txt");
            break;

        case 5: // LDA
            lda_params = loadLDAParams("5lda_params.txt");
            break;

        case 7: // 集成决策树（中位数）
            if (dtree_load_params_choice("7e_dtree_params1.txt", tree7_1, &n7_1) != 0 ||
                dtree_load_params_choice("7e_dtree_params2.txt", tree7_2, &n7_2) != 0 ||
                dtree_load_params_choice("7e_dtree_params3.txt", tree7_3, &n7_3) != 0)
            {
                fprintf(stderr, "加载集成决策树参数失败！\n");
                load_success = 0;
            }
            break;

        case 8: // 梯度提升决策树
            if (dtree_load_params_choice("8g_dtree_params1.txt", tree8_1, &n8_1) != 0 ||
                dtree_load_params_choice("8g_dtree_params2.txt", tree8_2, &n8_2) != 0 ||
                dtree_load_params_choice("8g_dtree_params3.txt", tree8_3, &n8_3) != 0)
            {
                fprintf(stderr, "加载梯度提升决策树参数失败！\n");
                load_success = 0;
            }
            break;

        case 11: // 融合1（决策树+贝叶斯+LDA）
            load_ret = dtree_load_params("2dtree_params.txt");
            if (load_ret != 0)
            {
                fprintf(stderr, "加载决策树参数失败！\n");
                load_success = 0;
                break;
            }
            nb_params = loadNBParams("4Bayes_params.txt");
            lda_params = loadLDAParams("5lda_params.txt");
            break;

        case 12: // 融合2（贝叶斯+LDA）
            nb_params = loadNBParams("4Bayes_params.txt");
            lda_params = loadLDAParams("5lda_params.txt");
            break;

        case 13: // 融合3（6模型投票）
            // 加载KNN
            if (load_train_X("1knn_train_X.txt") != 0 || load_train_Y("1knn_train_Y.txt") != 0)
            {
                fprintf(stderr, "加载KNN数据失败！\n");
                load_success = 0;
                break;
            }
            // 加载基础决策树
            load_ret = dtree_load_params("2dtree_params.txt");
            if (load_ret != 0)
            {
                fprintf(stderr, "加载决策树参数失败！\n");
                load_success = 0;
                break;
            }
            // 加载SVM
            svm_model = load_svm_model("3svm_params.txt");
            if (svm_model == NULL)
            {
                fprintf(stderr, "加载SVM模型失败！\n");
                load_success = 0;
                break;
            }
            // 加载贝叶斯
            nb_params = loadNBParams("4Bayes_params.txt");
            // 加载LDA
            lda_params = loadLDAParams("5lda_params.txt");
            // 加载集成决策树（中位数）
            if (dtree_load_params_choice("7e_dtree_params1.txt", tree7_1, &n7_1) != 0 ||
                dtree_load_params_choice("7e_dtree_params2.txt", tree7_2, &n7_2) != 0 ||
                dtree_load_params_choice("7e_dtree_params3.txt", tree7_3, &n7_3) != 0)
            {
                fprintf(stderr, "加载集成决策树参数失败！\n");
                load_success = 0;
            }
            break;
        }

        // 模型加载失败则跳过本次算法，继续下一个
        if (!load_success)
        {
            // 释放已加载的部分资源
            if (svm_model != NULL)
                free_svm_model(svm_model);
            if (user_choice == 4 || user_choice == 11 || user_choice == 12 || user_choice == 13)
                freeNBParams(&nb_params);
            if (user_choice == 5 || user_choice == 11 || user_choice == 12 || user_choice == 13)
                freeLDAParams(&lda_params);
            printf("算法 [%d] - %s 执行失败，跳过！\n", user_choice, algo_name);
            continue;
        }

        // ========== 初始化混淆矩阵 ==========
        int tp = 0, tn = 0, fp = 0, fn = 0, total = 0;

        // ========== 读取测试数据并预测 ==========
        FILE *file = fopen("TestSetData.txt", "r");
        if (file == NULL)
        {
            perror("无法打开测试数据文件");
            // 提前释放已加载的资源
            if (svm_model != NULL)
                free_svm_model(svm_model);
            if (user_choice == 4 || user_choice == 11 || user_choice == 12 || user_choice == 13)
                freeNBParams(&nb_params);
            if (user_choice == 5 || user_choice == 11 || user_choice == 12 || user_choice == 13)
                freeLDAParams(&lda_params);
            printf("算法 [%d] - %s 执行失败，跳过！\n", user_choice, algo_name);
            continue;
        }

        double features[FEAT_DIM];
        int true_label, final_predicted = -1;
        char line[256];

        printf("开始处理测试数据...\n");
        while (fgets(line, sizeof(line), file))
        {
            // 解析单行数据（6特征+1标签）
            int parse_ret = sscanf(line, "%lf %lf %lf %lf %lf %lf %d",
                                   &features[0], &features[1], &features[2],
                                   &features[3], &features[4], &features[5],
                                   &true_label);
            if (parse_ret != 7)
            {
                fprintf(stderr, "数据格式错误，跳过此行: %s", line);
                continue;
            }

            // ========== 核心：根据当前算法执行对应预测逻辑 ==========
            switch (user_choice)
            {
            case 1: // KNN
                final_predicted = knn_predict(features);
                break;

            case 2: // 基础决策树
                final_predicted = dtree_predict(features);
                break;

            case 3: // SVM
                final_predicted = svm_predict(svm_model, features);
                break;

            case 4: // 朴素贝叶斯
                final_predicted = (int)nbPredict(&nb_params, features);
                break;

            case 5: // LDA
                final_predicted = (int)ldaPredict(&lda_params, features);
                break;

            case 7: // 集成决策树（中位数）
                int p7_1 = dtree_predict_choice(tree7_1, n7_1, features);
                int p7_2 = dtree_predict_choice(tree7_2, n7_2, features);
                int p7_3 = dtree_predict_choice(tree7_3, n7_3, features);
                final_predicted = getMedian(p7_1, p7_2, p7_3);
                break;

            case 8: // 梯度提升决策树
                int p8_1 = dtree_predict_choice(tree8_1, n8_1, features);
                int p8_2 = dtree_predict_choice(tree8_2, n8_2, features);
                int p8_3 = dtree_predict_choice(tree8_3, n8_3, features);
                final_predicted = gboosting_predict(p8_1, p8_2, p8_3);
                break;

            case 11: // 融合1
                int p11_1 = dtree_predict(features);
                int p11_2 = (int)nbPredict(&nb_params, features);
                int p11_3 = (int)ldaPredict(&lda_params, features);
                final_predicted = fuseSingleSample1(p11_1, p11_2, p11_3);
                break;

            case 12: // 融合2
                int p12_1 = (int)nbPredict(&nb_params, features);
                int p12_2 = (int)ldaPredict(&lda_params, features);
                final_predicted = fuseSingleSample2(p12_1, p12_2);
                break;

            case 13: // 融合3（投票）
                int p13_1 = knn_predict(features);
                int p13_2 = dtree_predict(features);
                int p13_3 = svm_predict(svm_model, features);
                int p13_4 = (int)nbPredict(&nb_params, features);
                int p13_5 = (int)ldaPredict(&lda_params, features);
                int p13_7_1 = dtree_predict_choice(tree7_1, n7_1, features);
                int p13_7_2 = dtree_predict_choice(tree7_2, n7_2, features);
                int p13_7_3 = dtree_predict_choice(tree7_3, n7_3, features);
                int p13_7 = getMedian(p13_7_1, p13_7_2, p13_7_3);
                final_predicted = modelVoteFusion(p13_1, p13_2, p13_3, p13_4, p13_5, p13_7);
                break;
            }

            // ========== 更新混淆矩阵 ==========
            if (true_label == 1)
            {
                if (final_predicted == 1)
                    tp++;
                else if (final_predicted == 0)
                    fn++;
                else
                {
                    fprintf(stderr, "预测结果无效（非0/1），跳过此行\n");
                    continue;
                }
            }
            else if (true_label == 0)
            {
                if (final_predicted == 0)
                    tn++;
                else if (final_predicted == 1)
                    fp++;
                else
                {
                    fprintf(stderr, "预测结果无效（非0/1），跳过此行\n");
                    continue;
                }
            }
            else
            {
                fprintf(stderr, "无效标签值: %d，跳过此行\n", true_label);
                continue;
            }
            total++;
        }

        // ========== 输出所有评估指标 ==========
        fclose(file);
        print_all_metrics(tp, tn, fp, fn, total);

        // ========== 释放本次算法的模型资源 ==========
        if (svm_model != NULL)
            free_svm_model(svm_model);
        if (user_choice == 4 || user_choice == 11 || user_choice == 12 || user_choice == 13)
            freeNBParams(&nb_params);
        if (user_choice == 5 || user_choice == 11 || user_choice == 12 || user_choice == 13)
            freeLDAParams(&lda_params);

        // 移除所有互动暂停操作，直接执行下一个算法
        printf("算法 [%d] - %s 执行完成！\n", user_choice, algo_name);
    }

    printf("\n=============== 所有算法执行完毕 ===============\n");
    return 0;
}