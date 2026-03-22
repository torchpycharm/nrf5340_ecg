#include "utils.h"

int gboosting_predict(double pred12, double pred13, double pred14)
{

    // 3. 计算集成原始预测值（与Matlab逻辑一致：0.5 + 三个模型预测值）
    double labels_pre = 0.5 + pred12 + pred13 + pred14;

    // 4. 二分类决策：>0.5→1，否则→0
    int ans = (labels_pre > 0.5) ? 1 : 0;
    return ans;
}