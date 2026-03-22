#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tensorflow/lite/core/c/common.h"
// 替换为集成模型的头文件
#include "tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_1_data.h"
#include "tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_1_int8_data.h"
#include "tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_2_data.h"
#include "tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_2_int8_data.h"
#include "tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_3_data.h"
#include "tensorflow/lite/micro/examples/Fnn_bagging/models/fnn_ensemble_model_3_int8_data.h"
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

// 模型数量配置
constexpr int kNumModels = 3;  // 3个集成模型
constexpr int kInputDim = 6;   // 输入维度

// 模型结构体：存储模型数据和量化参数
typedef struct {
  const unsigned char* float_model_data;  // 浮点模型数据
  const unsigned char* int8_model_data;   // INT8量化模型数据
  float input_scale;                      // INT8输入缩放因子
  int input_zero_point;                   // INT8输入零点
  float output_scale;                     // INT8输出缩放因子
  int output_zero_point;                  // INT8输出零点
} EnsembleModel;

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
  *test_features = (float*)malloc(count * kInputDim * sizeof(float));
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
      (*test_features)[idx * kInputDim + 0] = f1;
      (*test_features)[idx * kInputDim + 1] = f2;
      (*test_features)[idx * kInputDim + 2] = f3;
      (*test_features)[idx * kInputDim + 3] = f4;
      (*test_features)[idx * kInputDim + 4] = f5;
      (*test_features)[idx * kInputDim + 5] = f6;
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

void InitEnsembleModels(EnsembleModel* models) {
  // 模型1配置
  models[0].float_model_data = g_fnn_ensemble_model_1_model_data;
  models[0].int8_model_data = g_fnn_ensemble_model_1_int8_model_data;

  // 模型2配置
  models[1].float_model_data = g_fnn_ensemble_model_2_model_data;
  models[1].int8_model_data = g_fnn_ensemble_model_2_int8_model_data;

  // 模型3配置
  models[2].float_model_data = g_fnn_ensemble_model_3_model_data;
  models[2].int8_model_data = g_fnn_ensemble_model_3_int8_model_data;
}

// 单个浮点模型推理
float RunFloatModelInference(const unsigned char* model_data,
                             const float* input_data,
                             FnnOpResolver& op_resolver) {
  const tflite::Model* model = ::tflite::GetModel(model_data);
  if (!model) {
    MicroPrintf("[错误] 无法加载浮点模型！");
    return -1.0f;
  }
  TFLITE_CHECK_EQ(model->version(), TFLITE_SCHEMA_VERSION);

  constexpr int kTensorArenaSize = 8192;
  uint8_t tensor_arena[kTensorArenaSize];

  tflite::MicroInterpreter interpreter(model, op_resolver, tensor_arena,
                                       kTensorArenaSize);

  TF_LITE_ENSURE_STATUS(interpreter.AllocateTensors());

  // 填充输入
  TfLiteTensor* input = interpreter.input(0);
  for (int j = 0; j < kInputDim; j++) {
    input->data.f[j] = input_data[j];
  }

  TF_LITE_ENSURE_STATUS(interpreter.Invoke());
  return interpreter.output(0)->data.f[0];
}

// 单个INT8模型推理
float RunInt8ModelInference(const unsigned char* model_data,
                            const float* input_data, FnnOpResolver& op_resolver,
                            float* input_scale, int* input_zero_point,
                            float* output_scale, int* output_zero_point) {
  const tflite::Model* model = ::tflite::GetModel(model_data);
  if (!model) {
    MicroPrintf("[错误] 无法加载INT8量化模型！");
    return -1.0f;
  }
  TFLITE_CHECK_EQ(model->version(), TFLITE_SCHEMA_VERSION);

  constexpr int kTensorArenaSize = 8192;
  uint8_t tensor_arena[kTensorArenaSize];

  tflite::MicroInterpreter interpreter(model, op_resolver, tensor_arena,
                                       kTensorArenaSize);

  TF_LITE_ENSURE_STATUS(interpreter.AllocateTensors());

  TfLiteTensor* input = interpreter.input(0);
  TfLiteTensor* output = interpreter.output(0);

  // 保存量化参数（首次调用时）
  if (input_scale != nullptr && input_zero_point != nullptr) {
    *input_scale = input->params.scale;
    *input_zero_point = input->params.zero_point;
    *output_scale = output->params.scale;
    *output_zero_point = output->params.zero_point;
  }

  // 量化输入：float -> int8
  int8_t input_int8[kInputDim];
  for (int j = 0; j < kInputDim; j++) {
    input_int8[j] = static_cast<int8_t>(
        round(input_data[j] / input->params.scale + input->params.zero_point));
  }

  // 填充输入
  for (int j = 0; j < kInputDim; j++) {
    input->data.int8[j] = input_int8[j];
  }

  TF_LITE_ENSURE_STATUS(interpreter.Invoke());

  // 反量化输出：int8 -> float
  return (output->data.int8[0] - output->params.zero_point) *
         output->params.scale;
}

// 集成模型投票推理（浮点版）
TfLiteStatus RunEnsembleFloatInference(const char* test_data_path,
                                       const float* mean, const float* scale) {
  MicroPrintf("\n===== 开始集成模型（浮点版）投票推理 =====");

  // 初始化模型配置
  EnsembleModel models[kNumModels];
  InitEnsembleModels(models);

  FnnOpResolver op_resolver;
  TF_LITE_ENSURE_STATUS(RegisterOps(op_resolver));

  // 加载测试集
  float* test_features;
  int* test_labels;
  int num_samples;
  if (!LoadTestData(test_data_path, &test_features, &test_labels,
                    &num_samples)) {
    return kTfLiteError;
  }

  // 单个模型指标 + 投票指标
  int single_TP[kNumModels] = {0};
  int single_TN[kNumModels] = {0};
  int single_FP[kNumModels] = {0};
  int single_FN[kNumModels] = {0};
  int vote_TP = 0, vote_TN = 0, vote_FP = 0, vote_FN = 0;

  MicroPrintf("开始批量推理测试，总计 %d 个样本", num_samples);

  for (int i = 0; i < num_samples; ++i) {
    float input[kInputDim];
    memcpy(input, &test_features[i * kInputDim], kInputDim * sizeof(float));
    NormalizeInput(input, mean, scale, kInputDim);

    // 每个模型独立推理
    int model_preds[kNumModels];
    float model_probs[kNumModels];

    for (int m = 0; m < kNumModels; m++) {
      float prob = RunFloatModelInference(models[m].float_model_data, input,
                                          op_resolver);
      model_probs[m] = prob;
      model_preds[m] = (prob > 0.5f) ? 1 : 0;

      // 更新单个模型指标
      int true_label = test_labels[i];
      if (true_label == 1 && model_preds[m] == 1) {
        single_TP[m]++;
      } else if (true_label == 0 && model_preds[m] == 0) {
        single_TN[m]++;
      } else if (true_label == 0 && model_preds[m] == 1) {
        single_FP[m]++;
      } else if (true_label == 1 && model_preds[m] == 0) {
        single_FN[m]++;
      }
    }

    // 投票逻辑：少数服从多数
    int vote_count = 0;
    for (int m = 0; m < kNumModels; m++) {
      vote_count += model_preds[m];
    }
    int final_pred = (vote_count > kNumModels / 2) ? 1 : 0;
    int true_label = test_labels[i];

    // 更新投票指标
    if (true_label == 1 && final_pred == 1) {
      vote_TP++;
    } else if (true_label == 0 && final_pred == 0) {
      vote_TN++;
    } else if (true_label == 0 && final_pred == 1) {
      vote_FP++;
    } else if (true_label == 1 && final_pred == 0) {
      vote_FN++;
    }
  }

  // 打印单个模型性能
  MicroPrintf("\n===== 单个浮点模型性能指标 =====");
  for (int m = 0; m < kNumModels; m++) {
    float accuracy = (float)(single_TP[m] + single_TN[m]) / num_samples;
    float precision = (single_TP[m] + single_FP[m]) > 0
                          ? (float)single_TP[m] / (single_TP[m] + single_FP[m])
                          : 0.0f;
    float recall = (single_TP[m] + single_FN[m]) > 0
                       ? (float)single_TP[m] / (single_TP[m] + single_FN[m])
                       : 0.0f;
    float f1 = (precision + recall) > 0
                   ? (2 * precision * recall) / (precision + recall)
                   : 0.0f;

    MicroPrintf("\n模型 %d 性能：", m + 1);
    MicroPrintf("  混淆矩阵：TP=%d, TN=%d, FP=%d, FN=%d", single_TP[m],
                single_TN[m], single_FP[m], single_FN[m]);
    MicroPrintf("  准确率: %.4f, 精确率: %.4f, 召回率: %.4f, F1: %.4f",
                accuracy, precision, recall, f1);
  }

  // 打印投票结果
  MicroPrintf("\n===== 投票后浮点模型性能指标 =====");
  float vote_accuracy = (float)(vote_TP + vote_TN) / num_samples;
  float vote_precision =
      (vote_TP + vote_FP) > 0 ? (float)vote_TP / (vote_TP + vote_FP) : 0.0f;
  float vote_recall =
      (vote_TP + vote_FN) > 0 ? (float)vote_TP / (vote_TP + vote_FN) : 0.0f;
  float vote_f1 =
      (vote_precision + vote_recall) > 0
          ? (2 * vote_precision * vote_recall) / (vote_precision + vote_recall)
          : 0.0f;

  MicroPrintf("混淆矩阵：TP=%d, TN=%d, FP=%d, FN=%d", vote_TP, vote_TN, vote_FP,
              vote_FN);
  MicroPrintf("准确率: %.4f, 精确率: %.4f, 召回率: %.4f, F1: %.4f",
              vote_accuracy, vote_precision, vote_recall, vote_f1);

  free(test_features);
  free(test_labels);
  return kTfLiteOk;
}

// 集成模型投票推理（INT8量化版）
TfLiteStatus RunEnsembleInt8Inference(const char* test_data_path,
                                      const float* mean, const float* scale) {
  MicroPrintf("\n===== 开始集成模型（INT8量化版）投票推理 =====");

  // 初始化模型配置
  EnsembleModel models[kNumModels];
  InitEnsembleModels(models);

  FnnOpResolver op_resolver;
  TF_LITE_ENSURE_STATUS(RegisterOps(op_resolver));

  // 加载测试集
  float* test_features;
  int* test_labels;
  int num_samples;
  if (!LoadTestData(test_data_path, &test_features, &test_labels,
                    &num_samples)) {
    return kTfLiteError;
  }

  // 初始化量化参数（首次运行时获取）
  float input_scale, output_scale;
  int input_zero_point, output_zero_point;

  // 单个模型指标 + 投票指标
  int single_TP[kNumModels] = {0};
  int single_TN[kNumModels] = {0};
  int single_FP[kNumModels] = {0};
  int single_FN[kNumModels] = {0};
  int vote_TP = 0, vote_TN = 0, vote_FP = 0, vote_FN = 0;

  MicroPrintf("开始批量推理测试，总计 %d 个样本", num_samples);

  for (int i = 0; i < num_samples; ++i) {
    float input[kInputDim];
    memcpy(input, &test_features[i * kInputDim], kInputDim * sizeof(float));
    NormalizeInput(input, mean, scale, kInputDim);

    // 每个模型独立推理
    int model_preds[kNumModels];
    float model_probs[kNumModels];

    for (int m = 0; m < kNumModels; m++) {
      float prob =
          RunInt8ModelInference(models[m].int8_model_data, input, op_resolver,
                                (i == 0) ? &input_scale : nullptr,
                                (i == 0) ? &input_zero_point : nullptr,
                                (i == 0) ? &output_scale : nullptr,
                                (i == 0) ? &output_zero_point : nullptr);
      model_probs[m] = prob;
      model_preds[m] = (prob > 0.5f) ? 1 : 0;

      // 更新单个模型指标
      int true_label = test_labels[i];
      if (true_label == 1 && model_preds[m] == 1) {
        single_TP[m]++;
      } else if (true_label == 0 && model_preds[m] == 0) {
        single_TN[m]++;
      } else if (true_label == 0 && model_preds[m] == 1) {
        single_FP[m]++;
      } else if (true_label == 1 && model_preds[m] == 0) {
        single_FN[m]++;
      }
    }

    // 投票逻辑：少数服从多数
    int vote_count = 0;
    for (int m = 0; m < kNumModels; m++) {
      vote_count += model_preds[m];
    }
    int final_pred = (vote_count > kNumModels / 2) ? 1 : 0;
    int true_label = test_labels[i];

    // 更新投票指标
    if (true_label == 1 && final_pred == 1) {
      vote_TP++;
    } else if (true_label == 0 && final_pred == 0) {
      vote_TN++;
    } else if (true_label == 0 && final_pred == 1) {
      vote_FP++;
    } else if (true_label == 1 && final_pred == 0) {
      vote_FN++;
    }
  }

  // 打印单个模型性能
  MicroPrintf("\n===== 单个INT8量化模型性能指标 =====");
  for (int m = 0; m < kNumModels; m++) {
    float accuracy = (float)(single_TP[m] + single_TN[m]) / num_samples;
    float precision = (single_TP[m] + single_FP[m]) > 0
                          ? (float)single_TP[m] / (single_TP[m] + single_FP[m])
                          : 0.0f;
    float recall = (single_TP[m] + single_FN[m]) > 0
                       ? (float)single_TP[m] / (single_TP[m] + single_FN[m])
                       : 0.0f;
    float f1 = (precision + recall) > 0
                   ? (2 * precision * recall) / (precision + recall)
                   : 0.0f;

    MicroPrintf("\n模型 %d 性能：", m + 1);
    MicroPrintf("  混淆矩阵：TP=%d, TN=%d, FP=%d, FN=%d", single_TP[m],
                single_TN[m], single_FP[m], single_FN[m]);
    MicroPrintf("  准确率: %.4f, 精确率: %.4f, 召回率: %.4f, F1: %.4f",
                accuracy, precision, recall, f1);
  }

  // 打印投票结果
  MicroPrintf("\n===== 投票后INT8量化模型性能指标 =====");
  float vote_accuracy = (float)(vote_TP + vote_TN) / num_samples;
  float vote_precision =
      (vote_TP + vote_FP) > 0 ? (float)vote_TP / (vote_TP + vote_FP) : 0.0f;
  float vote_recall =
      (vote_TP + vote_FN) > 0 ? (float)vote_TP / (vote_TP + vote_FN) : 0.0f;
  float vote_f1 =
      (vote_precision + vote_recall) > 0
          ? (2 * vote_precision * vote_recall) / (vote_precision + vote_recall)
          : 0.0f;

  MicroPrintf(
      "量化参数：输入缩放=%.6f, 输入零点=%d, 输出缩放=%.6f, 输出零点=%d",
      input_scale, input_zero_point, output_scale, output_zero_point);
  MicroPrintf("混淆矩阵：TP=%d, TN=%d, FP=%d, FN=%d", vote_TP, vote_TN, vote_FP,
              vote_FN);
  MicroPrintf("准确率: %.4f, 精确率: %.4f, 召回率: %.4f, F1: %.4f",
              vote_accuracy, vote_precision, vote_recall, vote_f1);

  free(test_features);
  free(test_labels);
  return kTfLiteOk;
}

// 性能分析（内存占用 + 推理耗时）
TfLiteStatus ProfileMemoryAndLatency() {
  MicroPrintf("\n===== 性能分析：模型内存和耗时 =====");

  tflite::MicroProfiler profiler;
  FnnOpResolver op_resolver;
  TF_LITE_ENSURE_STATUS(RegisterOps(op_resolver));

  // 使用模型1进行性能分析
  const unsigned char* model_data = g_fnn_ensemble_model_1_model_data;
  const tflite::Model* model = ::tflite::GetModel(model_data);
  if (!model) {
    MicroPrintf("[错误] 无法加载模型进行性能分析！");
    return kTfLiteError;
  }

  // 调整 Arena 大小适配 FNN 模型
  constexpr int kTensorArenaSize = 8192;
  uint8_t tensor_arena[kTensorArenaSize];
  constexpr int kNumResourceVariables = 24;

  tflite::RecordingMicroAllocator* allocator(
      tflite::RecordingMicroAllocator::Create(tensor_arena, kTensorArenaSize));
  tflite::RecordingMicroInterpreter interpreter(
      model, op_resolver, allocator,
      tflite::MicroResourceVariables::Create(allocator, kNumResourceVariables),
      &profiler);

  TF_LITE_ENSURE_STATUS(interpreter.AllocateTensors());
  TFLITE_CHECK_EQ(interpreter.inputs_size(), 1);

  // 全局标准化参数
  extern float g_Mean[6];
  extern float g_Scale[6];

  // 填充均值作为测试输入
  float test_input[6] = {g_Mean[0], g_Mean[1], g_Mean[2],
                         g_Mean[3], g_Mean[4], g_Mean[5]};
  NormalizeInput(test_input, g_Mean, g_Scale, 6);
  for (int i = 0; i < 6; i++) {
    interpreter.input(0)->data.f[i] = test_input[i];
  }

  TF_LITE_ENSURE_STATUS(interpreter.Invoke());

  MicroPrintf("\n耗时统计：");
  profiler.LogTicksPerTagCsv();

  MicroPrintf("\n内存分配详情：");
  interpreter.GetMicroAllocator().PrintAllocations();

  return kTfLiteOk;
}

// 全局标准化参数
float g_Mean[6];
float g_Scale[6];
}  // namespace

// 主函数：执行完整测试流程
int main(int argc, char* argv[]) {
  MicroPrintf("FNN Bagging 集成模型测试工具 - 启动");
  MicroPrintf("集成模型数量：%d", kNumModels);

  // 初始化目标平台
  tflite::InitializeTarget();
  // 参数初始化
  InitScalerParams(g_Mean, g_Scale);

  // 测试数据路径（更新为Fnn_bagging目录）
  const char* test_data_path =
      "/home/lixinying/tflite-micro/tensorflow/lite/micro/"
      "examples/Fnn_bagging/test_set.csv";

  // 执行性能分析
  TF_LITE_ENSURE_STATUS(ProfileMemoryAndLatency());

  // 执行浮点模型集成投票推理
  TF_LITE_ENSURE_STATUS(
      RunEnsembleFloatInference(test_data_path, g_Mean, g_Scale));

  // 执行INT8量化模型集成投票推理
  TF_LITE_ENSURE_STATUS(
      RunEnsembleInt8Inference(test_data_path, g_Mean, g_Scale));

  MicroPrintf("\n所有测试完成！");
  return kTfLiteOk;
}