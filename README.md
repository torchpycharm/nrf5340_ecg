# ECG分类系统 - nRF5340 NCS项目

## 项目概述

这是一个基于Nordic nRF5340和NCS (Nordic Connect SDK)的ECG信号分类系统。系统支持**两种通信模式**：

- **SPI模式（主）**: 实时流处理，持续接收树莓派通过SPI发送的ECG数据帧，采用环形缓冲和滑动窗口处理，无内存溢出风险
- **UART模式（辅）**: 用于调试和VOFA可视化，接收单次数据进行分类

## 系统架构

```
树莓派 (上位机)
    |
    ├─ SPI (主模式，流式发送)
    │   ↓
    │  nRF5340 (SPI从机)
    │   |
    │   ├─ SPI接收 (spi_comm.c)
    │   ├─ 流处理管道 (stream_processor.c)
    │   │   ├─ 环形缓冲区
    │   │   ├─ 特征提取 (feature_extraction.c)
    │   │   │   ├─ 峰度 (Kurtosis)
    │   │   │   └─ 偏度 (Skewness)
    │   │   └─ 决策树推理 (model_inference.c)
    │   └─ 结果输出 (日志)
    │
    └─ UART (辅助，调试用)
        ↓
       nRF5340 (UART从机)
        |
        └─ 用于VOFA可视化
```

## 文件结构

```
nrf5340_ncs_project/
├── CMakeLists.txt                      # CMake构建配置
├── prj.conf                            # 项目配置文件
├── nrf5340dk_nrf5340_cpuapp.overlay   # 设备树覆盖文件
├── README.md                           # 本文件
├── include/                            # 头文件目录
│   ├── uart_comm.h                    # UART通信接口（保持不变）
│   ├── spi_comm.h                     # SPI通信接口（新增）
│   ├── stream_processor.h             # 流处理管道接口（新增）
│   ├── feature_extraction.h           # 特征提取接口
│   ├── model_inference.h              # 模型推理接口
│   └── model_params.h                 # 模型参数定义
└── src/                                # 源文件目录
    ├── main.c                         # 主程序
    ├── uart_comm.c                    # UART通信实现（保持不变）
    ├── spi_comm.c                     # SPI通信实现（新增）
    ├── stream_processor.c             # 流处理管道实现（新增）
    ├── feature_extraction.c           # 特征提取实现
    ├── model_inference.c              # 模型推理实现
    └── model_params.c                 # 模型参数数据
```

## 功能模块说明

### 1. UART通信模块 (`uart_comm.c`) ✓ 保持不变

- **功能**: 与PC通过UART进行通信
- **用途**: VOFA实时可视化调试
- **特点**: 中断驱动，不影响SPI工作

### 2. SPI通信模块 (`spi_comm.c`) ⭐ 新增

**SPI从机接收**:
- 工作模式: SPI从机 (Slave)
- 波特率: 1 MHz
- 接收缓冲: 512字节，实时处理

**SPI帧格式** (来自树莓派):
```
Byte 0-1:   同步头 (0xAA55, little-endian)
Byte 2-3:   帧索引 (u16 LE)
Byte 4-5:   样本数量 (u16 LE)
Byte 6+:    ECG数据 (int16 LE，最多500个样本)
Byte -2:    CRC16校验和 (u16 LE)
```

**特点**:
- 实时处理，无需等待完整数据
- CRC16校验确保数据完整性
- 自动帧同步和错误恢复

### 3. 流处理管道 (`stream_processor.c`) ⭐ 新增

**关键特性**:
- **环形缓冲区**: 避免内存溢出，支持持续流处理
- **滑动窗口**: 每1024个样本处理一次
- **步长处理**: 512个样本的步长，支持重叠窗口
- **实时处理**: 接收即处理，无需存储全部数据

**处理流程**:
```
SPI帧 -> 环形缓冲区 -> 积累1024样本 -> 特征提取 -> 决策树 -> 结果输出
                                    ↓ 滑动512样本
                          (重复处理下一窗口)
```

**内存效率**:
- 环形缓冲区: 2048个float = ~8KB
- 临时窗口: 1024个float = ~4KB
- 总计: < 20KB RAM

### 4. 特征提取模块 (`feature_extraction.c`) ✓ 保持原功能

- 计算信号峰度 (Kurtosis)
- 计算信号偏度 (Skewness)
- 分三段计算，得到6个特征

### 5. 决策树推理模块 (`model_inference.c`) ✓ 保持原功能

- 特征标准化 (Z-score)
- 决策树遍历
- 二分类输出

## 编译和烧录

### 前置要求

1. 安装 [nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nrf-connect-sdk)
2. 安装 [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools)
3. 准备 nRF5340 DK 开发板

### 编译步骤

```bash
# 进入项目目录
cd nrf5340_ncs_project

# 使用west构建
west build -b nrf5340dk_nrf5340_cpuapp

# 烧录到开发板
west flash
```

### 查看日志

```bash
# 使用串口工具查看日志 (波特率115200)
# Windows: PuTTY, TeraTerm
# Linux/Mac: minicom, screen

# 示例 (Linux)
screen /dev/ttyACM0 115200
```

## 使用说明

### 1. 硬件连接

- 将nRF5340 DK通过USB连接到PC
- 将nRF5340的UART引脚连接到树莓派:
  - TX (P0.20) → 树莓派 RX
  - RX (P0.22) → 树莓派 TX
  - GND → GND

## 联系方式

如有问题或需要进一步的参考资料，请随时联系。

---
