load("//tensorflow/lite/micro:build_def.bzl", "generate_cc_arrays")

package(
    default_visibility = ["//visibility:public"],
)

exports_files(
    srcs = [
        "fnn_binary_classification.tflite",      
        "fnn_binary_classification_int8.tflite", 
    ],
    visibility = [
        "//tensorflow/lite/micro/examples/Fnn:__subpackages__", 
    ],
)

generate_cc_arrays(
    name = "generated_fnn_binary_classification_model_cc",
    src = "fnn_binary_classification.tflite",
    out = "fnn_binary_classification_model_data.cc",
)

generate_cc_arrays(
    name = "generated_fnn_binary_classification_model_hdr",
    src = "fnn_binary_classification.tflite",
    out = "fnn_binary_classification_model_data.h",
)

generate_cc_arrays(
    name = "generated_fnn_binary_classification_int8_model_cc",
    src = "fnn_binary_classification_int8.tflite",
    out = "fnn_binary_classification_int8_model_data.cc",
)

generate_cc_arrays(
    name = "generated_fnn_binary_classification_int8_model_hdr",
    src = "fnn_binary_classification_int8.tflite",
    out = "fnn_binary_classification_int8_model_data.h",
)