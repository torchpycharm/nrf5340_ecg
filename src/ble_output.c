/**
 * @file ble_output.c
 * @brief 蓝牙输出队列实现
 */

#include "ble_output.h"
#include <string.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/services/nus.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(ble_output, LOG_LEVEL_INF);

#define BLE_STREAM_THREAD_STACK_SIZE   2048
#define BLE_STREAM_THREAD_PRIORITY     8
#define BLE_STREAM_PACKET_INTERVAL_MS  5

#define DEVICE_NAME                    CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN                (sizeof(DEVICE_NAME) - 1)

typedef struct {
    bool active;
    bool start_sent;
    uint16_t file_id;
    uint32_t sample_count;
    uint32_t send_index;
    uint16_t fs_hz;
    uint8_t integrity_percent;
    int32_t model_output;
    int16_t samples[BLE_OUTPUT_MAX_WINDOW_SAMPLES];
} ble_stream_window_t;

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

/* ===== BLE NUS 发送链路 ===== */
static struct bt_conn *ble_conn;
static bool ble_started;
static bool ble_connected;

static ble_stream_window_t stream_window;
static struct k_mutex stream_lock;
static struct k_sem stream_sem;

static struct k_thread stream_thread_data;
static K_THREAD_STACK_DEFINE(stream_thread_stack, BLE_STREAM_THREAD_STACK_SIZE);
static bool stream_thread_started;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

static bool raw_baseline_mode_enabled(void)
{
    return IS_ENABLED(CONFIG_ECG_RAW_BLE_BASELINE);
}

static void put_u16_le(uint8_t *dst, uint16_t v)
{
    dst[0] = (uint8_t)(v & 0xFF);
    dst[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void put_u32_le(uint8_t *dst, uint32_t v)
{
    dst[0] = (uint8_t)(v & 0xFF);
    dst[1] = (uint8_t)((v >> 8) & 0xFF);
    dst[2] = (uint8_t)((v >> 16) & 0xFF);
    dst[3] = (uint8_t)((v >> 24) & 0xFF);
}

static int send_start_packet(const ble_stream_window_t *w)
{
    uint8_t pkt[12];

    pkt[0] = BLE_STREAM_PKT_START;
    pkt[1] = BLE_STREAM_PKT_VERSION;
    put_u16_le(&pkt[2], w->file_id);
    put_u32_le(&pkt[4], w->sample_count);
    put_u16_le(&pkt[8], w->fs_hz);
    pkt[10] = w->integrity_percent;
    pkt[11] = (uint8_t)(w->model_output & 0xFF);

    int ret = bt_nus_send(NULL, pkt, sizeof(pkt));
    if (raw_baseline_mode_enabled() && ret == -ENOTCONN) {
        return 0;
    }

    return ret;
}

static int send_data_packet(ble_stream_window_t *w)
{
    uint32_t remaining = w->sample_count - w->send_index;
    uint8_t chunk_limit = BLE_STREAM_CHUNK_SAMPLES;

    uint8_t chunk_count = (remaining > chunk_limit) ?
                          chunk_limit :
                          (uint8_t)remaining;
    uint8_t pkt[7 + (BLE_STREAM_CHUNK_SAMPLES * 2)];
    uint16_t start_idx = (uint16_t)w->send_index;
    uint16_t len = (uint16_t)(7 + (chunk_count * 2));

    pkt[0] = BLE_STREAM_PKT_DATA;
    pkt[1] = BLE_STREAM_PKT_VERSION;
    put_u16_le(&pkt[2], w->file_id);
    put_u16_le(&pkt[4], start_idx);
    pkt[6] = chunk_count;

    for (uint8_t i = 0; i < chunk_count; i++) {
        uint16_t pos = (uint16_t)(7 + (i * 2));
        put_u16_le(&pkt[pos], (uint16_t)w->samples[w->send_index + i]);
    }

    int ret = bt_nus_send(NULL, pkt, len);
    if (ret == 0 || (raw_baseline_mode_enabled() && ret == -ENOTCONN)) {
        w->send_index += chunk_count;
        return 0;
    }

    return ret;
}

static int send_end_packet(const ble_stream_window_t *w)
{
    uint8_t pkt[8];

    pkt[0] = BLE_STREAM_PKT_END;
    pkt[1] = BLE_STREAM_PKT_VERSION;
    put_u16_le(&pkt[2], w->file_id);
    put_u16_le(&pkt[4], (uint16_t)w->send_index);
    pkt[6] = w->integrity_percent;
    pkt[7] = (uint8_t)(w->model_output & 0xFF);

    int ret = bt_nus_send(NULL, pkt, sizeof(pkt));
    if (raw_baseline_mode_enabled() && ret == -ENOTCONN) {
        return 0;
    }

    return ret;
}

static void stream_sender_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    LOG_INF("BLE stream sender thread started");

    while (1) {
        k_sem_take(&stream_sem, K_FOREVER);

        while (1) {
            int ret;

            k_mutex_lock(&stream_lock, K_FOREVER);
            bool active = stream_window.active;
            uint32_t send_index = stream_window.send_index;
            uint16_t file_id = stream_window.file_id;
            k_mutex_unlock(&stream_lock);

            if (!active) {
                break;
            }

            if (!ble_connected && !raw_baseline_mode_enabled()) {
                k_sleep(K_MSEC(200));
                continue;
            }

            k_mutex_lock(&stream_lock, K_FOREVER);
            if (!stream_window.start_sent) {
                ret = send_start_packet(&stream_window);
                if (ret == 0) {
                    stream_window.start_sent = true;
                    LOG_INF("BLE START sent: file=%u samples=%u",
                            stream_window.file_id,
                            stream_window.sample_count);
                }
                k_mutex_unlock(&stream_lock);

                if (ret != 0) {
                    LOG_WRN("BLE START send failed: %d", ret);
                    k_sleep(K_MSEC(10));
                }
                continue;
            }

            if (stream_window.send_index < stream_window.sample_count) {
                ret = send_data_packet(&stream_window);
                uint32_t now_sent = stream_window.send_index;
                uint32_t total = stream_window.sample_count;
                uint16_t now_file = stream_window.file_id;
                k_mutex_unlock(&stream_lock);

                if (ret != 0) {
                    LOG_WRN("BLE DATA send failed: file=%u idx=%u ret=%d",
                            now_file, send_index, ret);
                    k_sleep(K_MSEC(10));
                    continue;
                }

                if ((now_sent % 600) == 0 || now_sent == total) {
                    LOG_INF("BLE DATA progress: file=%u %u/%u",
                            now_file, now_sent, total);
                }

                k_sleep(K_MSEC(BLE_STREAM_PACKET_INTERVAL_MS));
                continue;
            }

            ret = send_end_packet(&stream_window);
            if (ret == 0) {
                LOG_INF("BLE END sent: file=%u sent=%u",
                        stream_window.file_id,
                        stream_window.send_index);
                stream_window.active = false;
                stream_window.start_sent = false;
                stream_window.sample_count = 0;
                stream_window.send_index = 0;
            } else {
                LOG_WRN("BLE END send failed: file=%u ret=%d", file_id, ret);
            }
            k_mutex_unlock(&stream_lock);

            if (ret != 0) {
                k_sleep(K_MSEC(10));
            }
        }
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (err) {
        LOG_ERR("BLE connection failed: 0x%02x", err);
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("BLE connected: %s", addr);

    if (ble_conn) {
        bt_conn_unref(ble_conn);
    }
    ble_conn = bt_conn_ref(conn);
    ble_connected = true;

    LOG_INF("BLE stream chunk=%u samples/pkt", BLE_STREAM_CHUNK_SAMPLES);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_WRN("BLE disconnected: %s reason=0x%02x", addr, reason);

    ble_connected = false;

    if (ble_conn) {
        bt_conn_unref(ble_conn);
        ble_conn = NULL;
    }

    int adv_ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (adv_ret != 0) {
        LOG_ERR("BLE re-advertising failed: %d", adv_ret);
    } else {
        LOG_INF("BLE re-advertising started");
    }
}

BT_CONN_CB_DEFINE(ble_conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

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
    memset(&stream_window, 0, sizeof(stream_window));
    k_mutex_init(&stream_lock);
    k_sem_init(&stream_sem, 0, 1);

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
    ble_connected = false;

    LOG_INF("BLE queue deinitialized");
}

int ble_output_start(void)
{
    if (ble_started) {
        return 0;
    }

    int ret = bt_enable(NULL);
    if (ret != 0) {
        LOG_ERR("bt_enable failed: %d", ret);
        return ret;
    }

    LOG_INF("Bluetooth initialized");

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        settings_load();
    }

    static struct bt_nus_cb nus_cb;
    ret = bt_nus_init(&nus_cb);
    if (ret != 0) {
        LOG_ERR("bt_nus_init failed: %d", ret);
        return ret;
    }

    ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (ret != 0) {
        LOG_ERR("Advertising start failed: %d", ret);
        return ret;
    }

    LOG_INF("BLE advertising started");

    if (!stream_thread_started) {
        k_thread_create(&stream_thread_data,
                        stream_thread_stack,
                        K_THREAD_STACK_SIZEOF(stream_thread_stack),
                        stream_sender_thread,
                        NULL, NULL, NULL,
                        K_PRIO_PREEMPT(BLE_STREAM_THREAD_PRIORITY),
                        0,
                        K_NO_WAIT);
        stream_thread_started = true;
    }

    ble_started = true;
    return 0;
}

bool ble_output_is_connected(void)
{
    return ble_connected;
}

int ble_output_submit_window(uint16_t file_id,
                             const int16_t *samples,
                             uint32_t sample_count,
                             uint16_t fs_hz,
                             uint8_t integrity_percent,
                             int32_t model_output)
{
    if (!samples || sample_count == 0 || sample_count > BLE_OUTPUT_MAX_WINDOW_SAMPLES) {
        return -EINVAL;
    }

    if (!ble_started) {
        return -EACCES;
    }

    k_mutex_lock(&stream_lock, K_FOREVER);
    if (stream_window.active) {
        k_mutex_unlock(&stream_lock);
        return -EBUSY;
    }

    memcpy(stream_window.samples, samples, sample_count * sizeof(int16_t));
    stream_window.file_id = file_id;
    stream_window.sample_count = sample_count;
    stream_window.send_index = 0;
    stream_window.fs_hz = fs_hz;
    stream_window.integrity_percent = integrity_percent;
    stream_window.model_output = model_output;
    stream_window.start_sent = false;
    stream_window.active = true;
    k_mutex_unlock(&stream_lock);

    LOG_INF("BLE window queued: file=%u samples=%u integrity=%u label=%d",
            file_id, sample_count, integrity_percent, model_output);

    k_sem_give(&stream_sem);
    return 0;
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
