/**
 * @file ble_output.c
 * @brief 蓝牙输出队列实现
 */

#include "ble_output.h"
#include <string.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_output, LOG_LEVEL_INF);

/* ===== 蓝牙消息队列 ===== */
static ble_message_t *ble_queue = NULL;
static uint32_t ble_queue_size = 0;
static uint32_t ble_queue_head = 0;
static uint32_t ble_queue_tail = 0;
static uint32_t ble_queue_count = 0;

/* ===== 统计 ===== */
static ble_stats_t ble_stats = {0};

/* ===== 回调函数 ===== */
static ble_send_callback_t ble_send_cb = NULL;

/* ===== 背景发送线程 ===== */
static struct k_thread bg_sender_thread_data;
static struct k_thread *bg_sender_thread = &bg_sender_thread_data;
static bool bg_sender_active = false;

/**
 * @brief 初始化蓝牙队列
 */
int ble_output_init(uint32_t queue_size)
{
    if (queue_size == 0) {
        LOG_ERR("Invalid queue size");
        return -EINVAL;
    }

    if (ble_queue) {
        LOG_WRN("BLE queue already initialized");
        return -EALREADY;
    }

    ble_queue = k_malloc(queue_size * sizeof(ble_message_t));
    if (!ble_queue) {
        LOG_ERR("Failed to allocate BLE queue (size=%u)", queue_size);
        return -ENOMEM;
    }

    ble_queue_size = queue_size;
    ble_queue_head = 0;
    ble_queue_tail = 0;
    ble_queue_count = 0;

    memset(&ble_stats, 0, sizeof(ble_stats));

    LOG_INF("BLE output queue initialized: size=%u", queue_size);
    return 0;
}

/**
 * @brief 重置队列
 */
void ble_output_reset(void)
{
    if (!ble_queue) return;

    ble_queue_head = 0;
    ble_queue_tail = 0;
    ble_queue_count = 0;

    LOG_DBG("BLE queue reset");
}

/**
 * @brief 销毁队列
 */
void ble_output_deinit(void)
{
    bg_sender_active = false;
    k_sleep(K_MSEC(100));  /* 等待后台线程停止 */

    if (ble_queue) {
        k_free(ble_queue);
        ble_queue = NULL;
    }

    ble_queue_size = 0;

    LOG_INF("BLE queue deinitialized");
}

/**
 * @brief 将ECG结果队列化
 */
int ble_output_queue_result(const ecg_result_t *result)
{
    if (!ble_queue || !result) {
        return -EINVAL;
    }

    if (ble_queue_count >= ble_queue_size) {
        LOG_WRN("BLE queue full, dropping oldest message");
        ble_queue_tail = (ble_queue_tail + 1) % ble_queue_size;
        ble_queue_count--;
    }

    /* 创建诊断消息 */
    ble_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.timestamp_ms = result->timestamp_ms;
    msg.msg_type = 1;  /* 诊断结果 */
    
    if (result->integrity.is_valid) {
        msg.status = 0;  /* 正常 */
    } else {
        msg.status = 2;  /* 质量不足 */
    }

    /* 填充数据 */
    for (int i = 0; i < 3; i++) {
        msg.data.diagnosis.kurtosis[i] = (float)result->kurtosis[i];
        msg.data.diagnosis.skewness[i] = (float)result->skewness[i];
    }
    msg.data.diagnosis.model_output = result->model_output;
    msg.data.diagnosis.window_integrity_percent = 
        (uint8_t)result->integrity.integrity_percent;

    /* 入队 */
    ble_queue[ble_queue_head] = msg;
    ble_queue_head = (ble_queue_head + 1) % ble_queue_size;
    ble_queue_count++;

    LOG_DBG("ECG result queued (queue_count=%u)", ble_queue_count);
    return 0;
}

/**
 * @brief 队列化警告消息
 */
int ble_output_queue_warning(uint8_t loss_percent, uint8_t severity)
{
    if (!ble_queue) {
        return -EINVAL;
    }

    if (ble_queue_count >= ble_queue_size) {
        LOG_WRN("BLE queue full");
        ble_queue_tail = (ble_queue_tail + 1) % ble_queue_size;
        ble_queue_count--;
    }

    ble_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.timestamp_ms = k_uptime_get_32();
    msg.msg_type = 2;  /* 警告 */
    msg.status = severity;
    msg.data.warning.loss_percent = loss_percent;
    msg.data.warning.severity = severity;

    ble_queue[ble_queue_head] = msg;
    ble_queue_head = (ble_queue_head + 1) % ble_queue_size;
    ble_queue_count++;

    LOG_WRN("Warning queued: loss=%u%%, severity=%u", loss_percent, severity);
    return 0;
}

/**
 * @brief 出队消息
 */
int ble_output_dequeue_message(ble_message_t *msg)
{
    if (!ble_queue || !msg) {
        return -EINVAL;
    }

    if (ble_queue_count == 0) {
        return -ENODATA;
    }

    *msg = ble_queue[ble_queue_tail];
    ble_queue_tail = (ble_queue_tail + 1) % ble_queue_size;
    ble_queue_count--;

    return 0;
}

/**
 * @brief 获取BLE统计
 */
ble_stats_t ble_output_get_stats(void)
{
    ble_stats.queued_count = ble_queue_count;
    return ble_stats;
}

/**
 * @brief 注册BLE发送回调
 */
void ble_output_register_send_callback(ble_send_callback_t callback)
{
    ble_send_cb = callback;
    LOG_INF("BLE send callback registered");
}

/**
 * @brief 后台发送线程（可选）
 */
static void ble_bg_sender_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    LOG_INF("BLE background sender thread started");

    while (bg_sender_active) {
        /* 每100ms检查一次队列 */
        k_sleep(K_MSEC(100));

        if (!ble_send_cb) {
            continue;
        }

        /* 尽可能发送所有待发送的消息 */
        while (ble_queue_count > 0) {
            ble_message_t msg;
            if (ble_output_dequeue_message(&msg) == 0) {
                /* 序列化并发送 */
                uint8_t payload[64];
                int len = 0;

                payload[len++] = msg.msg_type;
                payload[len++] = msg.status;
                payload[len++] = (msg.timestamp_ms >> 0) & 0xFF;
                payload[len++] = (msg.timestamp_ms >> 8) & 0xFF;
                payload[len++] = (msg.timestamp_ms >> 16) & 0xFF;
                payload[len++] = (msg.timestamp_ms >> 24) & 0xFF;

                /* 根据类型打包数据 */
                if (msg.msg_type == 1) {  /* 诊断 */
                    payload[len++] = msg.data.diagnosis.window_integrity_percent;
                    /* 特征数据可按需压缩... */
                }

                int ret = ble_send_cb(payload, len);
                if (ret != 0) {
                    LOG_ERR("BLE send failed: %d", ret);
                    ble_stats.send_errors++;
                } else {
                    ble_stats.total_sent++;
                }
            }
        }
    }

    LOG_INF("BLE background sender thread stopped");
}

/**
 * @brief 启动后台发送线程
 */
int ble_output_start_background_sender(void)
{
    if (bg_sender_active) {
        LOG_WRN("BLE background sender already active");
        return -EALREADY;
    }

    bg_sender_active = true;

    /* 创建并启动线程 
     * 优先级: K_PRIO_PREEMPT(8) 为较低优先级，不阻塞主处理
     */
    static K_THREAD_STACK_DEFINE(ble_sender_stack, 1024);
    k_thread_create(
        &bg_sender_thread_data,
        ble_sender_stack,
        K_THREAD_STACK_SIZEOF(ble_sender_stack),
        ble_bg_sender_thread,
        NULL, NULL, NULL,
        K_PRIO_PREEMPT(8),
        0,
        K_NO_WAIT
    );
    bg_sender_thread = &bg_sender_thread_data;

    if (!bg_sender_thread) {
        LOG_ERR("Failed to create BLE sender thread");
        bg_sender_active = false;
        return -ENOMEM;
    }

    LOG_INF("BLE background sender started");
    return 0;
}

/**
 * @brief 手动发送待发送消息
 */
uint32_t ble_output_send_pending(void)
{
    if (!ble_send_cb) {
        return 0;
    }

    uint32_t sent_count = 0;

    while (ble_queue_count > 0) {
        ble_message_t msg;
        if (ble_output_dequeue_message(&msg) == 0) {
            uint8_t payload[64];
            int len = 4;  /* 预留头部 */

            payload[0] = msg.msg_type;
            payload[1] = msg.status;
            payload[2] = (msg.timestamp_ms >> 0) & 0xFF;
            payload[3] = (msg.timestamp_ms >> 8) & 0xFF;

            int ret = ble_send_cb(payload, len);
            if (ret == 0) {
                sent_count++;
                ble_stats.total_sent++;
            } else {
                ble_stats.send_errors++;
            }
        }
    }

    return sent_count;
}
