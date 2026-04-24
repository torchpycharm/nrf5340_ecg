#include "utils.h"
int fuseSingleSample2(int ped_class4_val, int ped_class5_val)
{

    // 原Matlab逻辑的C语言实现
    if (ped_class4_val == 0)
    {
        // 若朴素贝叶斯预测为0，最终结果直接为0
        return 0;
    }
    else
    {
        // 若朴素贝叶斯预测为1，需结合判别分析的结果
        if (ped_class5_val == 1)
        {
            // 判别分析也预测为1，最终结果为1
            return 1;
        }
        else
        {
            // 判别分析预测不为1，最终结果为0
            return 0;
        }
    }
}
