# fyp_integrate_int8 (INT8-only)

这个目录只保留 INT8 推理（不包含任何浮点相关的代码/参数）。

## 构建
`gcc -O2 -std=c11 -Wall -Wextra -Werror *.c -o fyp_integrate_int8.exe`

## 运行
- 评测全部 10 个模型：`./fyp_integrate_int8.exe all TestSetData.txt`
- 评测单个模型（1..10）：`./fyp_integrate_int8.exe 3 TestSetData.txt`

## 模型 ID（1..10）
1) knn
2) dtree
3) svm
4) bayes
5) lda
6) 7e_ensemble
7) 8g_gboost
8) mix1
9) mix2
10) mix3

## 数据格式
每行：`x1 x2 x3 x4 x5 x6 label`
- `x1..x6` 支持整数或小数（运行时会解析成 Q16.16 的 `int32_t`）
- `label`：0/1
