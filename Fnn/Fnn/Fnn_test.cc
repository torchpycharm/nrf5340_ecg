#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/examples/Fnn/models/fnn_binary_classification_int8_model_data.h"
#include "tensorflow/lite/micro/examples/Fnn/models/fnn_binary_classification_model_data.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_profiler.h"
#include "tensorflow/lite/micro/recording_micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {
// 关键：模板参数 = 你要注册的算子总数，这里是 2 个：全连接 + Logistic
using FnnOpResolver = tflite::MicroMutableOpResolver<2>;

TfLiteStatus RegisterOps(FnnOpResolver& op_resolver) {
  // 1. 先注册 FULLY_CONNECTED
  TF_LITE_ENSURE_STATUS(op_resolver.AddFullyConnected());
  // 2. 再注册 LOGISTIC
  TF_LITE_ENSURE_STATUS(op_resolver.AddLogistic());
  return kTfLiteOk;
}

// 标准化参数
void InitScalerParams(float* mean, float* scale) {
  // 训练集特征均值 mean_
  mean[0] = 15.0445f;
  mean[1] = 16.3455f;
  mean[2] = 16.6089f;
  mean[3] = 1.4722f;
  mean[4] = 1.5089f;
  mean[5] = 1.6104f;

  // 训练集特征标准差 scale_
  scale[0] = 12.3295f;
  scale[1] = 11.9176f;
  scale[2] = 15.3273f;
  scale[3] = 2.5767f;
  scale[4] = 2.7871f;
  scale[5] = 2.7538f;
}

// 读取 test_set.csv
bool LoadTestData(const char* file_path, float** test_features,
                  int** test_labels, int* num_samples) {
  FILE* fp = fopen(file_path, "r");
  if (!fp) {
    MicroPrintf("[错误] 无法打开测试集文件，请检查路径和权限！");
    return false;
  }

  // 统计样本数
  char buf[1024];
  int count = 0;
  fgets(buf, sizeof(buf), fp);  // 跳过表头
  while (fgets(buf, sizeof(buf), fp)) {
    count++;
  }
  *num_samples = count;

  // 分配内存
  *test_features = (float*)malloc(count * 6 * sizeof(float));
  *test_labels = (int*)malloc(count * sizeof(int));
  if (!*test_features || !*test_labels) {
    MicroPrintf("[错误] 测试数据内存分配失败！内存不足或样本数过大");
    fclose(fp);
    return false;
  }

  // 读取数据
  rewind(fp);
  fgets(buf, sizeof(buf), fp);  // 跳过表头
  int idx = 0;
  int parse_error = 0;
  while (fgets(buf, sizeof(buf), fp)) {
    float f1, f2, f3, f4, f5, f6;
    int label;
    if (sscanf(buf, "%f,%f,%f,%f,%f,%f,%d", &f1, &f2, &f3, &f4, &f5, &f6,
               &label) == 7) {
      (*test_features)[idx * 6 + 0] = f1;
      (*test_features)[idx * 6 + 1] = f2;
      (*test_features)[idx * 6 + 2] = f3;
      (*test_features)[idx * 6 + 3] = f4;
      (*test_features)[idx * 6 + 4] = f5;
      (*test_features)[idx * 6 + 5] = f6;
      (*test_labels)[idx] = label;
      idx++;
    } else {
      parse_error++;
    }
  }
  fclose(fp);

  if (parse_error > 0) {
    MicroPrintf("[警告] 测试集解析时发现 %d 行格式错误，请检查CSV格式！",
                parse_error);
  }
  return true;
}

// 标准化输入特征
void NormalizeInput(float* input, const float* mean, const float* scale,
                    int dim) {
  for (int i = 0; i < dim; i++) {
    input[i] = (input[i] - mean[i]) / scale[i];
  }
}

// 全局标准化参数
float g_Mean[6];
float g_Scale[6];
}  // namespace

// 性能分析（内存占用 + 推理耗时）
TfLiteStatus ProfileMemoryAndLatency() {
  MicroPrintf("1.性能分析：分析模型内存和耗时");

  tflite::MicroProfiler profiler;
  FnnOpResolver op_resolver;
  TF_LITE_ENSURE_STATUS(RegisterOps(op_resolver));

  // 调整 Arena 大小适配 FNN 模型
  constexpr int kTensorArenaSize = 8192;
  uint8_t tensor_arena[kTensorArenaSize];
  constexpr int kNumResourceVariables = 24;

  tflite::RecordingMicroAllocator* allocator(
      tflite::RecordingMicroAllocator::Create(tensor_arena, kTensorArenaSize));
  tflite::RecordingMicroInterpreter interpreter(
      tflite::GetModel(g_fnn_binary_classification_model_data), op_resolver,
      allocator,
      tflite::MicroResourceVariables::Create(allocator, kNumResourceVariables),
      &profiler);

  TF_LITE_ENSURE_STATUS(interpreter.AllocateTensors());
  TFLITE_CHECK_EQ(interpreter.inputs_size(), 1);

  // 填充均值作为测试输入
  float test_input[6] = {g_Mean[0], g_Mean[1], g_Mean[2],
                         g_Mean[3], g_Mean[4], g_Mean[5]};
  NormalizeInput(test_input, g_Mean, g_Scale, 6);
  for (int i = 0; i < 6; i++) {
    interpreter.input(0)->data.f[i] = test_input[i];
  }

  TF_LITE_ENSURE_STATUS(interpreter.Invoke());

  MicroPrintf("\n1.1 耗时统计：");
  profiler.LogTicksPerTagCsv();

  MicroPrintf("\n1.2内存分配详情：");
  interpreter.GetMicroAllocator().PrintAllocations();

  return kTfLiteOk;
}

// 浮点模型推理测试
TfLiteStatus LoadFloatModelAndPerformInference() {
  MicroPrintf("2.开始加载并测试浮点模型");

  const tflite::Model* model =
      ::tflite::GetModel(g_fnn_binary_classification_model_data);
  if (!model) {
    MicroPrintf("[错误] 无法加载浮点模型，请检查模型头文件！");
    return kTfLiteError;
  }
  TFLITE_CHECK_EQ(model->version(), TFLITE_SCHEMA_VERSION);

  FnnOpResolver op_resolver;
  TF_LITE_ENSURE_STATUS(RegisterOps(op_resolver));

  constexpr int kTensorArenaSize = 8192;
  uint8_t tensor_arena[kTensorArenaSize];

  tflite::MicroInterpreter interpreter(model, op_resolver, tensor_arena,
                                       kTensorArenaSize);

  TF_LITE_ENSURE_STATUS(interpreter.AllocateTensors());

  // 加载测试集
  float* test_features;
  int* test_labels;
  int num_samples;
  if (!LoadTestData("/home/lixinying/tflite-micro/tensorflow/lite/micro/"
                    "examples/Fnn/test_set.csv",
                    &test_features, &test_labels, &num_samples)) {
    return kTfLiteError;
  }

  int TP = 0, TN = 0, FP = 0, FN = 0;

  // 统计准确率
  MicroPrintf("2.1 开始批量推理测试");
  for (int i = 0; i < num_samples; ++i) {
    float input[6];
    memcpy(input, &test_features[i * 6], 6 * sizeof(float));
    NormalizeInput(input, g_Mean, g_Scale, 6);

    // 填充输入
    for (int j = 0; j < 6; j++) {
      interpreter.input(0)->data.f[j] = input[j];
    }

    TF_LITE_ENSURE_STATUS(interpreter.Invoke());
    float y_pred = interpreter.output(0)->data.f[0];
    int pred_label = (y_pred > 0.5f) ? 1 : 0;
    int true_label = test_labels[i];

    // 更新混淆矩阵指标
    if (true_label == 1 && pred_label == 1) {
      TP++;
    } else if (true_label == 0 && pred_label == 0) {
      TN++;
    } else if (true_label == 0 && pred_label == 1) {
      FP++;
    } else if (true_label == 1 && pred_label == 0) {
      FN++;
    }
  }

  // 计算核心指标（修改重点）
  float accuracy = (float)(TP + TN) / num_samples;  // 准确率
  float sensitivity =
      (TP + FN) > 0 ? (float)TP / (TP + FN) : 0.0f;  // 灵敏度（召回率）
  float specificity = (TN + FP) > 0 ? (float)TN / (TN + FP) : 0.0f;  // 特异度
  float precision = (TP + FP) > 0 ? (float)TP / (TP + FP) : 0.0f;    // 精确率
  float f1_score =
      (precision + sensitivity) > 0
          ? (2 * precision * sensitivity) / (precision + sensitivity)
          : 0.0f;  // F1值

  // 输出混淆矩阵
  MicroPrintf("2.2 混淆矩阵：");
  MicroPrintf("----------------------------------------");
  MicroPrintf("              预测值");
  MicroPrintf("            1       0");
  MicroPrintf("实际值 1    %d      %d  ", TP, FN);
  MicroPrintf("      0    %d      %d  ", FP, TN);

  // 输出指定格式的评估指标（修改重点）
  MicroPrintf("\n2.3 测试完成！总计 %d 个样本", num_samples);
  MicroPrintf("准确率(Accuracy): %.4f ", accuracy);
  MicroPrintf("灵敏度(Sensitivity): %.4f ", sensitivity);
  MicroPrintf("特异度(Specificity): %.4f ", specificity);
  MicroPrintf("精确率(Precision): %.4f ", precision);
  MicroPrintf("F1-score: %.4f ", f1_score);

  free(test_features);
  free(test_labels);

  return kTfLiteOk;
}

// INT8量化模型推理测试
TfLiteStatus LoadQuantModelAndPerformInference() {
  MicroPrintf("3.开始加载并测试INT8量化模型");

  const tflite::Model* model =
      ::tflite::GetModel(g_fnn_binary_classification_int8_model_data);
  if (!model) {
    MicroPrintf("[错误] 无法加载量化模型，请检查模型头文件！");
    return kTfLiteError;
  }
  TFLITE_CHECK_EQ(model->version(), TFLITE_SCHEMA_VERSION);

  FnnOpResolver op_resolver;
  TF_LITE_ENSURE_STATUS(RegisterOps(op_resolver));

  constexpr int kTensorArenaSize = 8192;
  uint8_t tensor_arena[kTensorArenaSize];

  tflite::MicroInterpreter interpreter(model, op_resolver, tensor_arena,
                                       kTensorArenaSize);

  TF_LITE_ENSURE_STATUS(interpreter.AllocateTensors());

  TfLiteTensor* input = interpreter.input(0);
  TFLITE_CHECK_NE(input, nullptr);
  TfLiteTensor* output = interpreter.output(0);
  TFLITE_CHECK_NE(output, nullptr);

  float output_scale = output->params.scale;
  int output_zero_point = output->params.zero_point;
  float input_scale = input->params.scale;
  int input_zero_point = input->params.zero_point;
  MicroPrintf("3.1 量化参数：");
  MicroPrintf("         输入缩放因子: %.6f, 输入零点: %d", input_scale,
              input_zero_point);
  MicroPrintf("         输出缩放因子: %.6f, 输出零点: %d", output_scale,
              output_zero_point);

  // 加载测试集
  float* test_features;
  int* test_labels;
  int num_samples;
  if (!LoadTestData("/home/lixinying/tflite-micro/tensorflow/lite/micro/"
                    "examples/Fnn/test_set.csv",
                    &test_features, &test_labels, &num_samples)) {
    return kTfLiteError;
  }

  int TP = 0, TN = 0, FP = 0, FN = 0;

  // 统计准确率
  MicroPrintf("3.2 开始批量推理测试");

  for (int i = 0; i < num_samples; ++i) {
    float input_float[6];
    memcpy(input_float, &test_features[i * 6], 6 * sizeof(float));
    NormalizeInput(input_float, g_Mean, g_Scale, 6);

    // 量化输入，float -> int8
    int8_t input_int8[6];
    for (int j = 0; j < 6; j++) {
      input_int8[j] = static_cast<int8_t>(
          round(input_float[j] / input_scale + input_zero_point));
    }

    // 填充输入
    for (int j = 0; j < 6; j++) {
      input->data.int8[j] = input_int8[j];
    }

    TF_LITE_ENSURE_STATUS(interpreter.Invoke());
    float y_pred = (output->data.int8[0] - output_zero_point) * output_scale;
    int pred_label = (y_pred > 0.5f) ? 1 : 0;
    int true_label = test_labels[i];

    // 更新混淆矩阵指标
    if (true_label == 1 && pred_label == 1) {
      TP++;
    } else if (true_label == 0 && pred_label == 0) {
      TN++;
    } else if (true_label == 0 && pred_label == 1) {
      FP++;
    } else if (true_label == 1 && pred_label == 0) {
      FN++;
    }
  }

  // 计算核心指标（修改重点）
  float accuracy = (float)(TP + TN) / num_samples;  // 准确率
  float sensitivity =
      (TP + FN) > 0 ? (float)TP / (TP + FN) : 0.0f;  // 灵敏度（召回率）
  float specificity = (TN + FP) > 0 ? (float)TN / (TN + FP) : 0.0f;  // 特异度
  float precision = (TP + FP) > 0 ? (float)TP / (TP + FP) : 0.0f;    // 精确率
  float f1_score =
      (precision + sensitivity) > 0
          ? (2 * precision * sensitivity) / (precision + sensitivity)
          : 0.0f;  // F1值

  // 输出混淆矩阵
  MicroPrintf("3.3 混淆矩阵：");
  MicroPrintf("----------------------------------------");
  MicroPrintf("              预测值");
  MicroPrintf("            1       0");
  MicroPrintf("实际值 1    %d      %d  ", TP, FN);
  MicroPrintf("      0    %d      %d  ", FP, TN);

  // 输出指定格式的评估指标（修改重点）
  MicroPrintf("\n3.4 测试完成！总计 %d 个样本", num_samples);
  MicroPrintf("准确率(Accuracy): %.4f ", accuracy);
  MicroPrintf("灵敏度(Sensitivity): %.4f ", sensitivity);
  MicroPrintf("特异度(Specificity): %.4f ", specificity);
  MicroPrintf("精确率(Precision): %.4f ", precision);
  MicroPrintf("F1-score: %.4f ", f1_score);

  free(test_features);
  free(test_labels);
  return kTfLiteOk;
}

// 主函数：执行完整测试流程
int main(int argc, char* argv[]) {
  MicroPrintf("FNN模型测试工具 - 启动");

  // 初始化目标平台
  tflite::InitializeTarget();
  // 参数初始化
  InitScalerParams(g_Mean, g_Scale);

  // 执行性能分析和推理测试
  TF_LITE_ENSURE_STATUS(ProfileMemoryAndLatency());
  TF_LITE_ENSURE_STATUS(LoadFloatModelAndPerformInference());
  TF_LITE_ENSURE_STATUS(LoadQuantModelAndPerformInference());

  MicroPrintf("所有测试完成！");
  return kTfLiteOk;
}