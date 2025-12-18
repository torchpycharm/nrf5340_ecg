# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ecg_classifier)

# 添加源文件
target_sources(app PRIVATE 
    src/main.c
    src/bsp_vofa.c
    src/spi_comm.c
    src/feature_extraction.c
    # src/model_inference.c
    # src/model_params.c
    src/online_moments.c
)

# 添加头文件路径
target_include_directories(app PRIVATE include)
