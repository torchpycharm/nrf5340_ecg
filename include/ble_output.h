/**
 * @file ble_output.h
 * @brief 蓝牙输出接口（预留）
 * 
 * 功能：
 * - 结果队列管理
 * - 蓝牙发送接口
 * - 数据格式转换
 */

#ifndef BLE_OUTPUT_H
#define BLE_OUTPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "ecg_processing.h"

/**
 * @brief 蓝牙消息体
 */
typedef struct {
    uint32_t timestamp_ms;      /* 消息时间戳 */
    
    uint8_t msg_type;           /* 消息类型: 1=诊断结果, 2=警告, 3=统计 */
    uint8_t status;             /* 状态码 */
    
    /* 数据包 - 可根据msg_type灵活解释 */
    union {
        struct {
            /* 诊断结果 */
            float kurtosis[3];
            float skewness[3];
            int32_t model_output;
            uint8_t window_integrity_percent;
        } diagnosis;
        
        struct {
            /* 数据质量警告 */
            uint8_t loss_percent;
            uint32_t missing_count;
            uint8_t severity;  /* 0=低, 1=中, 2=高 */
        } warning;
        
        struct {
            /* 统计信息 */
            uint32_t total_frames;
            uint32_t total_lost;
            uint32_t windows_valid;
            uint32_t windows_invalid;
        } stats;
    } data;
} ble_message_t;

/**
 * @brief 初始化蓝牙队列
 * @param queue_size 队列容量
 * @return 0成功
 */
int ble_output_init(uint32_t queue_size);

/**
 * @brief 清除蓝牙队列
 */
void ble_output_reset(void);

/**
 * @brief 销毁蓝牙队列（释放资源）
 */
void ble_output_deinit(void);

/**
 * @brief 将ECG处理结果加入发送队列
 * @param result ECG处理结果
 * @return 0成功，<0失败
 */
int ble_output_queue_result(const ecg_result_t *result);

/**
 * @brief 将诊断警告加入发送队列
 * @param loss_percent 丢帧率
 * @param severity 严重级别 (0/1/2)
 * @return 0成功
 */
int ble_output_queue_warning(uint8_t loss_percent, uint8_t severity);

/**
 * @brief 从队列中取出消息（待发送）
 * @param msg 输出消息指针
 * @return 0成功，-ENODATA队列空
 */
int ble_output_dequeue_message(ble_message_t *msg);

/**
 * @brief 获取队列统计
 */
typedef struct {
    uint32_t queued_count;      /* 待发送消息数 */
    uint32_t total_sent;        /* 已发送总数（仅当BLE已集成时） */
    uint32_t send_errors;       /* 发送错误数 */
} ble_stats_t;

ble_stats_t ble_output_get_stats(void);

/**
 * @brief 实际蓝牙发送函数接口
 * 
 * 这个函数需要用户根据蓝牙栈自行实现（如NRF5 SDK的GATT）
 * 作为回调注册给ble_output模块
 */
typedef int (*ble_send_callback_t)(const uint8_t *data, uint16_t length);

/**
 * @brief 注册蓝牙发送回调
 * 
 * 使用示例：
 * @code
 * int my_ble_send(const uint8_t *data, uint16_t len) {
 *     // 通过NRF SDK的notify/indicate机制发送
 *     return ble_gatt_notify(data, len);
 * }
 * 
 * ble_output_register_send_callback(my_ble_send);
 * @endcode
 */
void ble_output_register_send_callback(ble_send_callback_t callback);

/**
 * @brief 启动后台蓝牙发送线程（可选）
 * 
 * 如果启用，蓝牙输出将自动从队列中取消息并发送
 * 否则需要应用层定期调用 ble_output_send_pending()
 */
int ble_output_start_background_sender(void);

/**
 * @brief 手动发送待发送的消息（如不使用后台线程）
 * @return 实际发送的消息数
 */
uint32_t ble_output_send_pending(void);

#endif /* BLE_OUTPUT_H */
