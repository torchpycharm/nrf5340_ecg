#ifndef SPI_COMM_H
#define SPI_COMM_H

#include <stdint.h>
#include <stdbool.h>
#include "ecg_buffer.h"

#define SPI_FRAME_SIZE 5

/**
 * @brief SPI帧格式（来自主机）
 */
typedef struct {
    uint8_t id;         /* Frame ID / 序列号低字节 */
    uint8_t ack;        /* Acknowledge or extended sequence */
    uint8_t data_l;     /* ECG data low byte */
    uint8_t data_h;     /* ECG data high byte */
    uint8_t type;       /* Frame type / status */
} spi_frame_t;

/**
 * @brief SPI通信初始化（线程接收模式）
 * 
 * 功能：
 * - 配置NRF5340 SPI2为Slave模式
 * - 创建专用接收线程
 * - 自动启动连续接收
 * 
 * @param buffer 外部传入的ECG缓冲队列指针
 * @return 0成功，<0失败
 */
int spi_comm_init_async(ecg_buffer_t *buffer);

/**
 * @brief 获取SPI统计信息
 */
typedef struct {
    uint32_t frames_received;   /* 接收帧数 */
    uint32_t crc_errors;        /* CRC错误数 */
    uint32_t buffer_overruns;   /* 缓冲溢出数 */
    uint32_t seq_gaps;          /* 序列号中断数 */
} spi_stats_t;

spi_stats_t spi_comm_get_stats(void);

#endif /* SPI_COMM_H */
