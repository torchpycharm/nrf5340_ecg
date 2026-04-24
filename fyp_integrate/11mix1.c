#include "utils.h"
int fuseSingleSample1(int ped_class2_val, int ped_class4_val, int ped_class5_val)
{
    int result = 0; // 初始值对应MATLAB的zeros初始化

    if (ped_class2_val == 1)
    { // 决策树预测为1
        if (ped_class4_val == 0)
        { // 朴素贝叶斯预测为0
            if (ped_class5_val == 1)
            { // 判别分析预测为1
                result = 0;
            }
            else
            {
                result = 1;
            }
        }
        else
        { // 朴素贝叶斯预测为1
            result = 1;
        }
    }
    // 若决策树预测不为1，结果保持初始值0

    return result;
}
