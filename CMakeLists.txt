# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Ensure TFLM is visible to Zephyr even though it is not in west manifest.
set(EXTRA_ZEPHYR_MODULES
    $ENV{ZEPHYR_BASE}/modules/tflite-micro
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ecg_classifier)

# 添加源文件
target_sources(app PRIVATE 
    src/main.c
    #src/bsp_vofa.c
    src/uart_sample_rx.c
    src/spi_comm.c
    src/ecg_buffer.c
    src/ecg_processing.c
    src/ble_output.c
    src/feature_extraction.c
    src/models/model_runtime.c
    src/models/model_backend_param_binary.c
    src/models/model_backend_fnn_gboost_float.cc
    src/models/model_backend_fnn_bagging_float.c
    src/models/fnn_gboost_model_data.c
    # src/model_inference.c
    # src/model_params.c
    src/model_params.c
    src/online_moments.c
)

# 添加头文件路径
target_include_directories(app PRIVATE include)
