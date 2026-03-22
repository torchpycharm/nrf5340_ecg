load("//tensorflow/lite/micro:build_def.bzl", "generate_cc_arrays")

package(
    default_visibility = ["//visibility:public"],
)

exports_files(
    srcs = [
        # 模型1：浮点版
        "gradient_boost_model_1.tflite",
        # 模型1：INT8量化版
        "gradient_boost_model_1_int8.tflite",
        
        # 模型2：浮点版
        "gradient_boost_model_2.tflite",
        # 模型2：INT8量化版
        "gradient_boost_model_2_int8.tflite",
        
        # 模型3：浮点版
        "gradient_boost_model_3.tflite",
        # 模型3：INT8量化版
        "gradient_boost_model_3_int8.tflite",
    ],
    visibility = [
        "//tensorflow/lite/micro/examples/Fnn_gboosting:__subpackages__", 
    ],
)

# -----------------------------------------------------------------------------
# 模型1 生成规则
# -----------------------------------------------------------------------------
generate_cc_arrays(
    name = "gradient_boost_model_1_cc",
    src = "gradient_boost_model_1.tflite",
    out = "gradient_boost_model_1_data.cc",
)

generate_cc_arrays(
    name = "gradient_boost_model_1_hdr",
    src = "gradient_boost_model_1.tflite",
    out = "gradient_boost_model_1_data.h",
)

generate_cc_arrays(
    name = "gradient_boost_model_1_int8_cc",
    src = "gradient_boost_model_1_int8.tflite",
    out = "gradient_boost_model_1_int8_data.cc",
)

generate_cc_arrays(
    name = "gradient_boost_model_1_int8_hdr",
    src = "gradient_boost_model_1_int8.tflite",
    out = "gradient_boost_model_1_int8_data.h",
)

# -----------------------------------------------------------------------------
# 模型2 生成规则
# -----------------------------------------------------------------------------
generate_cc_arrays(
    name = "gradient_boost_model_2_cc",
    src = "gradient_boost_model_2.tflite",
    out = "gradient_boost_model_2_data.cc",
)

generate_cc_arrays(
    name = "gradient_boost_model_2_hdr",
    src = "gradient_boost_model_2.tflite",
    out = "gradient_boost_model_2_data.h",
)

generate_cc_arrays(
    name = "gradient_boost_model_2_int8_cc",
    src = "gradient_boost_model_2_int8.tflite",
    out = "gradient_boost_model_2_int8_data.cc",
)

generate_cc_arrays(
    name = "gradient_boost_model_2_int8_hdr",
    src = "gradient_boost_model_2_int8.tflite",
    out = "gradient_boost_model_2_int8_data.h",
)

# -----------------------------------------------------------------------------
# 模型3 生成规则
# -----------------------------------------------------------------------------
generate_cc_arrays(
    name = "gradient_boost_model_3_cc",
    src = "gradient_boost_model_3.tflite",
    out = "gradient_boost_model_3_data.cc",
)

generate_cc_arrays(
    name = "gradient_boost_model_3_hdr",
    src = "gradient_boost_model_3.tflite",
    out = "gradient_boost_model_3_data.h",
)

generate_cc_arrays(
    name = "gradient_boost_model_3_int8_cc",
    src = "gradient_boost_model_3_int8.tflite",
    out = "gradient_boost_model_3_int8_data.cc",
)

generate_cc_arrays(
    name = "gradient_boost_model_3_int8_hdr",
    src = "gradient_boost_model_3_int8.tflite",
    out = "gradient_boost_model_3_int8_data.h",
)