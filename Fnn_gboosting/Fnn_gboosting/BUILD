load("@rules_python//python:defs.bzl", "py_binary")
load("@tflm_pip_deps//:requirements.bzl", "requirement")
load(
    "//tensorflow/lite/micro:build_def.bzl",
    "tflm_cc_library",
    "tflm_cc_test",
)

package(
    # Disabling layering_check because of http://b/177257332
    features = ["-layering_check"],
    licenses = ["notice"],
)

tflm_cc_library(
    name = "model",
    srcs = [
        # 模型1 - 浮点版 + INT8量化版（梯度提升模型）
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_1_cc",
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_1_int8_cc",
        # 模型2 - 浮点版 + INT8量化版（梯度提升模型）
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_2_cc",
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_2_int8_cc",
        # 模型3 - 浮点版 + INT8量化版（梯度提升模型）
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_3_cc",
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_3_int8_cc",
    ],
    hdrs = [
        # 模型1 - 浮点版 + INT8量化版（梯度提升模型）
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_1_hdr",
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_1_int8_hdr",
        # 模型2 - 浮点版 + INT8量化版（梯度提升模型）
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_2_hdr",
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_2_int8_hdr",
        # 模型3 - 浮点版 + INT8量化版（梯度提升模型）
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_3_hdr",
        "//tensorflow/lite/micro/examples/Fnn_gboosting/models:gradient_boost_model_3_int8_hdr",
    ],
)

tflm_cc_test(
    name = "Fnn_gboosting_test",
    srcs = [
        "Fnn_gboosting_test.cc",  # 同步修改测试文件名
    ],
    deps = [
        ":model",
        "//tensorflow/lite/micro:micro_framework",
        "//tensorflow/lite/micro:micro_log",
        "//tensorflow/lite/micro:micro_profiler",
        "//tensorflow/lite/micro:op_resolvers",
        "//tensorflow/lite/micro:recording_allocators",
        "//tensorflow/lite/micro/testing:micro_test",
        "//tensorflow/lite/schema:schema_fbs",
    ],
)

py_binary(
    name = "train",
    srcs = ["train.py"],  # 对应修改后的梯度提升训练脚本
    data = [
        "training_set.csv",
        "test_set.csv",
    ],
    deps = [
        requirement("absl_py"),    
        requirement("numpy"),      
        requirement("tensorflow"), 
        requirement("keras"),      
    ],
)