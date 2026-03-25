# nRF5340 ECG Classification Project

## Overview

This repository contains an ECG signal classification project for the Nordic nRF5340 using Nordic Connect SDK and Zephyr.

The main firmware includes:

- ECG sample reception over SPI and UART
- Signal buffering and preprocessing
- Feature extraction and window management
- Lightweight on-device model inference
- BLE and VOFA-related output support

The repository also contains model-training assets, reference classifier implementations, helper scripts, and a committed build output directory.

## Repository Structure

```text
nrf5340_ecg/
|-- CMakeLists.txt
|-- prj.conf
|-- README.md
|-- boards/
|   `-- nrf5340dk_nrf5340_cpuapp_ns.overlay
|-- build/
|   |-- ... generated NCS / Zephyr build files
|   `-- nrf5340_ecg/
|       `-- ... application-specific build output
|-- include/
|   |-- ble_output.h
|   |-- bsp_vofa.h
|   |-- ecg_buffer.h
|   |-- ecg_processing.h
|   |-- feature_extraction.h
|   |-- feature_window.h
|   |-- model_inference.h
|   |-- model_params.h
|   |-- online_moments.h
|   |-- spi_comm.h
|   |-- uart_sample_rx.h
|   `-- models/
|       |-- fnn_gboost_model_data.h
|       `-- model_runtime.h
|-- src/
|   |-- ble_output.c
|   |-- bsp_vofa.c
|   |-- ecg_buffer.c
|   |-- ecg_processing.c
|   |-- feature_extraction.c
|   |-- feature_window.c
|   |-- main.c
|   |-- model_inference.c
|   |-- model_params.c
|   |-- online_moments.c
|   |-- spi_comm.c
|   |-- uart_sample_rx.c
|   `-- models/
|       |-- fnn_gboost_model_data.c
|       |-- model_backends.h
|       |-- model_backend_fnn_bagging_float.c
|       |-- model_backend_fnn_gboost_float.cc
|       |-- model_backend_param_binary.c
|       `-- model_runtime.c
|-- tools/
|   |-- rpi_uart_sender.py
|   `-- sim.py
|-- Fnn_bagging/
|   `-- Fnn_bagging/
|       |-- BUILD
|       |-- Fnn_bagging_test.cc
|       |-- Makefile.inc
|       |-- README.md
|       |-- train.py
|       |-- training_set.csv
|       |-- test_set.csv
|       |-- models/
|       `-- quantization/
|-- Fnn_gboosting/
|   `-- Fnn_gboosting/
|       |-- BUILD
|       |-- Fnn_gboosting_test.cc
|       |-- Makefile.inc
|       |-- README.md
|       |-- train.py
|       |-- training_set.csv
|       |-- test_set.csv
|       |-- models/
|       `-- quantization/
`-- fyp_integrate/
  `-- fyp_integrate/
    |-- multiple reference C implementations
    |-- parameter text files
    |-- test data files
    `-- utility and experiment files
```

## Directory Notes

- `boards/`: Board-specific device tree overlay for the nRF5340 target.
- `include/`: Public headers for acquisition, processing, inference, and communication.
- `src/`: Main firmware implementation.
- `include/models/` and `src/models/`: Embedded model data and runtime/backend integration.
- `tools/`: Host-side helper scripts for simulation and UART data sending.
- `Fnn_bagging/`: Training and evaluation assets for the bagging-based model variant.
- `Fnn_gboosting/`: Training and evaluation assets for the gradient-boosting-based model variant.
- `fyp_integrate/`: Older standalone experiments and reference classifier implementations in C.
- `build/`: Generated build artifacts currently present in the repository.

## Main Firmware Modules

- `main.c`: Firmware entry point.
- `spi_comm.c`: SPI sample reception.
- `uart_sample_rx.c`: UART sample input path.
- `ecg_buffer.c` and `feature_window.c`: Buffering and window management.
- `ecg_processing.c` and `feature_extraction.c`: Signal processing and feature computation.
- `model_inference.c` and `src/models/`: On-device inference pipeline and model backends.
- `ble_output.c` and `bsp_vofa.c`: Output and visualization support.

## Build Context

This is an NCS / Zephyr CMake project for the nRF5340 platform. The root build configuration is defined by `CMakeLists.txt`, `prj.conf`, and the overlay in `boards/`.
