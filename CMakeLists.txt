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
    src/uart_sample_rx.c
    src/spi_comm.c
    src/ecg_buffer.c
    src/ecg_processing.c
    src/ble_output.c
)

if(NOT CONFIG_ECG_RAW_BLE_MODE)
    target_sources(app PRIVATE
        src/feature_extraction.c
        src/models/model_runtime.c
        src/models/model_backend_fnn_gboost_float.cc
        src/models/model_backend_fnn_float.cc
        src/models/model_backend_fnn_bagging_float.cc
        src/models/model_backend_fnn_gboost_int8.cc
        src/models/model_backend_fnn_bagging_int8.cc
        src/models/model_backend_fnn_int8.cc
        src/models/model_backend_binary_classic.c
        src/models/model_backend_binary_int8.c
        src/models/fnn_gboost_model_data.c
        src/models/fnn_float_model_data.c
        src/models/fnn_bagging_float_model_data.c
        src/models/fnn_gboost_int8_model_data.c
        src/models/fnn_int8_model_data.c
        src/models/fnn_bagging_int8_model_data.c
        fyp_integrate_int8/model_int8_params.c
        fyp_integrate_int8/utils_int8.c
        fyp_integrate/func.c
        fyp_integrate/model_globals.c
        fyp_integrate/1knn.c
        fyp_integrate/2dtree.c
        fyp_integrate/3svm.c
        fyp_integrate/4Bayes.c
        fyp_integrate/5lda.c
        fyp_integrate/7e_dtree.c
        fyp_integrate/8g_dtree.c
        fyp_integrate/11mix1.c
        fyp_integrate/12mix2.c
        fyp_integrate/13mix3.c
        src/online_moments.c
    )
endif()

# 添加头文件路径
target_include_directories(app PRIVATE include)
