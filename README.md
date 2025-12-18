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

### 2. 树莓派端配置

**需要你提供树莓派端的数据发送代码**，参考格式:

```python
# 示例Python代码 (需要根据实际情况调整)
import serial
import struct

def send_ecg_data(ecg_samples, sampling_rate):
    ser = serial.Serial('/dev/ttyAMA0', 115200)
    
    # 构建数据包
    packet = bytearray()
    packet.append(0xAA)  # 起始字节
    packet.extend(struct.pack('<H', len(ecg_samples)))  # 采样点数
    packet.extend(struct.pack('<H', sampling_rate))     # 采样率
    
    # 添加ECG数据
    for sample in ecg_samples:
        packet.extend(struct.pack('<f', sample))
    
    # 计算校验和
    checksum = sum(packet) & 0xFFFF
    packet.extend(struct.pack('<H', checksum))
    packet.append(0x55)  # 结束字节
    
    ser.write(packet)
    ser.close()
```

### 3. 运行流程

1. 启动nRF5340程序
2. 树莓派发送ECG数据
3. nRF5340接收并处理数据
4. nRF5340返回分类结果
5. 查看日志确认结果

## 待完善部分

### 需要你提供的信息:

1. **树莓派数据传输协议**
   - 数据包格式
   - 通信波特率
   - 数据编码方式

2. **ECG数据格式**
   - 采样率范围
   - 数据长度范围
   - 数据单位和量程

3. **测试数据**
   - 提供几组真实ECG数据用于测试
   - 验证特征提取的正确性

### 可能需要调整的部分:

1. **UART缓冲区大小** (`uart_comm.h`)
   - 当前设置为4096个采样点
   - 根据实际数据长度调整

2. **特征计算精度**
   - 验证峰度和偏度计算是否与MATLAB一致
   - 可能需要微调算法

3. **内存优化**
   - 如果数据量大，可能需要流式处理
   - 减少内存占用

## 调试技巧

### 1. 启用详细日志

修改 `prj.conf`:
```
CONFIG_LOG_DEFAULT_LEVEL=4  # 改为4启用DEBUG级别日志
```

### 2. 测试特征提取

在 `main.c` 中添加测试代码:
```c
// 使用已知数据测试
float test_signal[100] = { /* 填入测试数据 */ };
feature_vector_t test_features;
extract_features(test_signal, 100, &test_features);
// 对比MATLAB结果
```

### 3. 验证决策树

使用PC端C代码 (`c_version/model_params.c`) 的测试数据:
```c
float x[6] = {16.2737, 16.3935, 15.5037, 3.2182, 3.2546, 3.2042};
feature_vector_t features = {
    .ksqi1 = x[0], .ksqi2 = x[1], .ksqi3 = x[2],
    .ssqi1 = x[3], .ssqi2 = x[4], .ssqi3 = x[5]
};
int result = tree_predict(&features);
// 应该输出1
```

## 性能指标

- **处理延迟**: < 100ms (取决于数据长度)
- **内存占用**: ~20KB RAM
- **Flash占用**: ~50KB

## 常见问题

### Q1: UART接收超时
- 检查波特率配置
- 检查硬件连接
- 确认树莓派正在发送数据

### Q2: 特征值异常
- 检查ECG数据是否正确接收
- 验证预处理步骤
- 对比MATLAB计算结果

### Q3: 分类结果不准确
- 验证模型参数是否正确导出
- 检查特征标准化是否正确
- 使用已知数据测试

## 下一步工作

1. **完善UART协议** - 根据树莓派端实现调整
2. **测试验证** - 使用真实数据测试
3. **性能优化** - 减少延迟和内存占用
4. **错误处理** - 增强鲁棒性
5. **功耗优化** - 添加低功耗模式

## 联系方式

如有问题或需要进一步的参考资料，请随时联系。

---

**注意**: 这是一个框架代码，部分功能需要根据实际硬件和协议进行调整。
