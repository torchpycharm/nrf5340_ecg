/**
 * @file ecg_buffer.h
 * @brief ECG数据FIFO环形缓冲队列管理
 * 
 * 功能：
 * - 脱耦SPI中断处理与主处理逻辑
 * - 防止主机快速发送导致丢帧
 * - 线程安全操作（K_SPINLOCK保护）
 */

#ifndef ECG_BUFFER_H
#define ECG_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
/**
 * @brief ECG样本数据单元
 * 注意：这是处理后的数据
 */
typedef struct {
    uint32_t seq_num;       /* 序列号，用于检测丢帧 */
    int16_t  ecg_data;      /* ECG原始样本值 */
    uint32_t timestamp_ms;  /* 接收时间戳 */
    uint8_t  status;        /* 帧状态标志 (0=正常, 1=重复, 2=乱序等) */
} ecg_sample_t;

/**
 * @brief ECG缓冲队列
 */
typedef struct {
    ecg_sample_t *buffer;
    uint32_t size;          /* 缓冲区大小 */
    uint32_t head;          /* 写指针（生产者） */
    uint32_t tail;          /* 读指针（消费者） */
    uint32_t count;         /* 当前队列中元素数 */
    
    /* 统计信息 */
    uint32_t total_received;    /* 总接收帧数 */
    uint32_t total_lost;        /* 总丢失帧数 */
    uint32_t last_seq;          /* 上一个序列号 */
} ecg_buffer_t;

/**
 * @brief 初始化ECG缓冲队列
 * @param buf 缓冲队列结构体指针
 * @param size 队列容量（推荐 >= (ECG_FS * 30 / 4)，即单个30s窗口的数据量）
 * @return 0成功，<0失败
 */
int ecg_buffer_init(ecg_buffer_t *buf, uint32_t size);

/**
 * @brief 清除缓冲队列（重置统计但保留内存）
 */
void ecg_buffer_reset(ecg_buffer_t *buf);

/**
 * @brief 销毁缓冲队列（释放内存）
 */
void ecg_buffer_deinit(ecg_buffer_t *buf);

/**
 * @brief 将ECG样本写入队列（由SPI异步回调调用）
 * @param buf 缓冲队列
 * @param sample ECG样本指针
 * @return 0成功，-ENOBUFS队列满（丢帧警告）
 */
int ecg_buffer_push(ecg_buffer_t *buf, const ecg_sample_t *sample);

/**
 * @brief 从队列读取一个ECG样本（由主处理线程调用）
 * @param buf 缓冲队列
 * @param sample 输出样本指针
 * @return 0成功，-ENODATA队列空
 */
int ecg_buffer_pop(ecg_buffer_t *buf, ecg_sample_t *sample);

/**
 * @brief 查询队列是否为空
 */
bool ecg_buffer_is_empty(const ecg_buffer_t *buf);

/**
 * @brief 查询队列是否满
 */
bool ecg_buffer_is_full(const ecg_buffer_t *buf);

/**
 * @brief 获取当前队列中的元素数
 */
uint32_t ecg_buffer_count(const ecg_buffer_t *buf);

/**
 * @brief 获取缓冲区使用率（百分比）
 */
uint8_t ecg_buffer_usage_percent(const ecg_buffer_t *buf);

/**
 * @brief 获取统计信息
 */
typedef struct {
    uint32_t total_received;
    uint32_t total_lost;
    uint8_t loss_percent;
} ecg_buffer_stats_t;

void ecg_buffer_get_stats(const ecg_buffer_t *buf, ecg_buffer_stats_t *stats);

#endif /* ECG_BUFFER_H */
