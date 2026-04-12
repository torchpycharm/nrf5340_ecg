/**
 * @file ecg_processing.h
 * @brief ECG数据处理流程管理
 * 
 * 功能：
 * - 30秒窗口完整性判断
 * - 数据处理流程协调
 * - 蓝牙输出队列预留
 */

#ifndef ECG_PROCESSING_H
#define ECG_PROCESSING_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 30秒窗口的完整性信息
 */
typedef struct {
    uint32_t expected_samples;      /* 期望样本数 */
    uint32_t received_samples;      /* 实际接收样本数 */
    uint32_t missing_samples;       /* 缺失样本数 */
    uint32_t seq_gaps;              /* 序列号中断次数 */
    uint32_t integrity_percent;     /* 完整性百分比 (0-100) */
    bool is_valid;                  /* 是否有效 (>=95%) */
    uint32_t window_start_time_ms;  /* 窗口开始时间 */
    uint32_t window_end_time_ms;    /* 窗口结束时间 */
    uint32_t window_duration_ms;    /* 实际持续时间 */
} window_integrity_t;

/**
 * @brief ECG处理结果（待发送的数据）
 */
typedef struct {
    uint32_t timestamp_ms;          /* 结果生成时间 */
    
    /* 特征 */
    double kurtosis[3];             /* 三段峰度 */
    double skewness[3];             /* 三段偏度 */
    
    /* 完整性检查 */
    window_integrity_t integrity;   /* 窗口完整性 */
    
    /* 推理结果（保留） */
    int32_t model_output;           /* 模型输出 */
    float model_confidence;         /* 置信度 */
    
    /* 状态标志 */
    uint8_t status;                 /* 0=OK, 1=丢帧警告, 2=质量不足 */
} ecg_result_t;

/**
 * @brief 30秒窗口管理器
 */
typedef struct {
    /* 配置 */
    uint32_t fs;                    /* 采样率 Hz */
    uint32_t window_sec;            /* 窗口长度 秒 */
    uint32_t expected_samples;      /* 期望样本数 = fs * window_sec */
    
    /* 状态 */
    uint32_t sample_count;          /* 当前窗口已处理样本数 */
    uint32_t seq_last;              /* 上一个序列号 */
    uint32_t seq_gaps;              /* 序列号中断检测 */
    uint32_t window_start_time_ms;  /* 窗口开始时间戳 */
    
    /* 统计 */
    uint32_t windows_completed;     /* 完成窗口数 */
    uint32_t windows_invalid;       /* 无效窗口数 */
} window_manager_t;

/**
 * @brief 初始化窗口管理器
 * @param mgr 窗口管理器指针
 * @param fs 采样率（Hz）
 * @param window_sec 窗口长度（秒）
 */
void window_manager_init(window_manager_t *mgr, uint32_t fs, uint32_t window_sec);

/**
 * @brief 更新当前窗口期望样本数（用于动态窗口）
 * @param mgr 窗口管理器
 * @param expected_samples 该窗口应接收样本数
 * @return 0成功，<0失败
 */
int window_manager_set_expected_samples(window_manager_t *mgr, uint32_t expected_samples);

/**
 * @brief 处理一个ECG样本，返回是否完成了一个窗口
 * @param mgr 窗口管理器
 * @param seq_num 序列号
 * @param integrity 输出：完整性信息（仅当返回true时有效）
 * @return true=完成一个窗口, false=继续积累
 */
bool window_manager_push(window_manager_t *mgr, uint32_t seq_num, 
                         window_integrity_t *integrity);

/**
 * @brief 重置窗口（开始新的30秒周期）
 */
void window_manager_reset(window_manager_t *mgr);

/**
 * @brief 获取当前窗口的完整性信息（不重置）
 */
void window_manager_get_integrity(const window_manager_t *mgr, 
                                   window_integrity_t *integrity);

/**
 * @brief 获取窗口管理器的统计信息
 */
typedef struct {
    uint32_t windows_completed;
    uint32_t windows_invalid;
    float validity_percent;
} window_stats_t;

void window_manager_get_stats(const window_manager_t *mgr, window_stats_t *stats);

#endif /* ECG_PROCESSING_H */
