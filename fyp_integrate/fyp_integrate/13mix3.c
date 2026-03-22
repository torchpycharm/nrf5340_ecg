#include "utils.h"
// 集成模型投票融合函数
int modelVoteFusion(int ped_class1_val, int ped_class2_val,
                    int ped_class3_val, int ped_class4_val,
                    int ped_class5_val, int ped_class7_val)
{
    // 1. 计算6个模型预测值的总和
    int sum_total = ped_class1_val + ped_class2_val +
                    ped_class3_val + ped_class4_val +
                    ped_class5_val + ped_class7_val;

    // 2. 计算6个模型的预测均值
    double mean_total = sum_total / 6.0;

    int result;
    if (mean_total > 0.5)
    {
        // 均值>0.5，初步分类为1
        result = 1;
    }
    else if (mean_total < 0.5)
    {
        // 均值<0.5，初步分类为0
        result = 0;
    }
    else
    {
        // 3. 处理平局（均值=0.5，即sum_total=3）：用模型1/3/7的预测值重新计算均值
        int sum_sub = ped_class1_val + ped_class3_val + ped_class7_val;
        double mean_sub = sum_sub / 3.0;
        // 重新决策：均值>0.5则为1，否则为0
        result = (mean_sub > 0.5) ? 1 : 0;
    }

    // 4. 最终确保输出为0/1（与Matlab中double(ped_class11 > 0.5)逻辑一致）
    return result;
}
