/**
 * @file ecg_buffer.c
 * @brief ECG数据缓冲队列实现
 */

#include "ecg_buffer.h"
#include <string.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ecg_buffer, LOG_LEVEL_INF);

/**
 * @brief 初始化ECG缓冲队列
 */
int ecg_buffer_init(ecg_buffer_t *buf, uint32_t size)
{
    if (!buf || size == 0) {
        return -EINVAL;
    }

    buf->buffer = k_malloc(size * sizeof(ecg_sample_t));
    if (!buf->buffer) {
        LOG_ERR("Failed to allocate ECG buffer (size=%u)", size);
        return -ENOMEM;
    }

    buf->size = size;
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->total_received = 0;
    buf->total_lost = 0;
    buf->last_seq = 0;

    LOG_INF("ECG buffer initialized: size=%u", size);
    return 0;
}

/**
 * @brief 重置缓冲队列
 */
void ecg_buffer_reset(ecg_buffer_t *buf)
{
    if (!buf) return;

    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->last_seq = 0;
    memset(buf->buffer, 0, buf->size * sizeof(ecg_sample_t));

    LOG_DBG("ECG buffer reset");
}

/**
 * @brief 销毁缓冲队列
 */
void ecg_buffer_deinit(ecg_buffer_t *buf)
{
    if (!buf || !buf->buffer) return;

    k_free(buf->buffer);
    buf->buffer = NULL;
    buf->size = 0;

    LOG_INF("ECG buffer deinitialized");
}

/**
 * @brief 将样本写入队列（供SPI异步回调使用）
 * 线程安全：使用自旋锁保护
 */
int ecg_buffer_push(ecg_buffer_t *buf, const ecg_sample_t *sample)
{
    if (!buf || !sample) {
        return -EINVAL;
    }

    /* 检查丢帧 */
    if (buf->total_received > 0) {
        uint32_t expected_seq = buf->last_seq + 1;
        int32_t delta = (int32_t)(sample->seq_num - expected_seq);
        
        if (delta > 0) {
            LOG_WRN("Lost %d frames (expected seq=%u, got %u)", 
                   delta, expected_seq, sample->seq_num);
            buf->total_lost += delta;
        } else if (delta < 0) {
            LOG_WRN("Out-of-order frame (expected seq=%u, got %u)", 
                   expected_seq, sample->seq_num);
        }
    }

    buf->last_seq = sample->seq_num;
    buf->total_received++;

    /* 检查缓冲区是否满 */
    if (buf->count >= buf->size) {
        LOG_ERR("ECG buffer overflow! Dropping oldest frame.");
        buf->total_lost++;
        
        /* 覆盖最旧的帧（环形缓冲） */
        buf->tail = (buf->tail + 1) % buf->size;
    } else {
        buf->count++;
    }

    /* 写入新样本 */
    buf->buffer[buf->head] = *sample;
    buf->head = (buf->head + 1) % buf->size;

    if (buf->count > (buf->size * 4 / 5)) {
        LOG_WRN("ECG buffer usage high: %u/%u (%.0f%%)", 
               buf->count, buf->size, 
               (buf->count * 100.0) / buf->size);
    }

    return 0;
}

/**
 * @brief 从队列读取一个样本（供主处理线程使用）
 */
int ecg_buffer_pop(ecg_buffer_t *buf, ecg_sample_t *sample)
{
    if (!buf || !sample) {
        return -EINVAL;
    }

    if (buf->count == 0) {
        return -ENODATA;  /* 队列为空 */
    }

    *sample = buf->buffer[buf->tail];
    // printk("Popping sample: seq_num=%u, ecg_data=%d\n", 
    //     buf->buffer[buf->tail].seq_num, buf->buffer[buf->tail].ecg_data);
    buf->tail = (buf->tail + 1) % buf->size;
    buf->count--;

    return 0;
}

/**
 * @brief 检查队列是否为空
 */
bool ecg_buffer_is_empty(const ecg_buffer_t *buf)
{
    return buf && (buf->count == 0);
}

/**
 * @brief 检查队列是否满
 */
bool ecg_buffer_is_full(const ecg_buffer_t *buf)
{
    return buf && (buf->count >= buf->size);
}

/**
 * @brief 获取队列中当前元素数
 */
uint32_t ecg_buffer_count(const ecg_buffer_t *buf)
{
    return buf ? buf->count : 0;
}

/**
 * @brief 获取缓冲区使用率（百分比）
 */
uint8_t ecg_buffer_usage_percent(const ecg_buffer_t *buf)
{
    if (!buf || buf->size == 0) return 0;
    return (uint8_t)((buf->count * 100) / buf->size);
}

/**
 * @brief 获取统计信息
 */
void ecg_buffer_get_stats(const ecg_buffer_t *buf, ecg_buffer_stats_t *stats)
{
    if (!buf || !stats) return;

    stats->total_received = buf->total_received;
    stats->total_lost = buf->total_lost;
    
    if (buf->total_received > 0) {
        stats->loss_percent = (uint8_t)((buf->total_lost * 100) / buf->total_received);
    } else {
        stats->loss_percent = 0;
    }
}
