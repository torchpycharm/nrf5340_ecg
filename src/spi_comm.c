#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include "spi_comm.h"
#include "ecg_buffer.h"

LOG_MODULE_REGISTER(spi_comm, CONFIG_LOG_DEFAULT_LEVEL);

/* 定义线程参数 */
#define SPI_RX_THREAD_STACK_SIZE 2048
#define SPI_RX_THREAD_PRIORITY   2  // 设置为较高的优先级以防止丢帧

static const struct device *spi_dev;
static struct spi_config spi_cfg;
// static ecg_buffer_t *p_ecg_buf = NULL; // 调试模式：暂不使用缓冲队列
struct k_thread spi_rx_thread_data;
K_THREAD_STACK_DEFINE(spi_rx_thread_stack, SPI_RX_THREAD_STACK_SIZE);

/* 统计信息 */
static spi_stats_t spi_stats = {0};

/* 静态接收缓冲，确保DMA安全 */
static uint8_t rx_raw_bytes[SPI_FRAME_SIZE];

/**
 * @brief SPI 接收线程函数
 */
void spi_rx_thread_entry(void *p1, void *p2, void *p3) {
    int ret;
    struct spi_buf rx_buf = { .buf = rx_raw_bytes, .len = SPI_FRAME_SIZE };
    struct spi_buf_set rx_bufs = { .buffers = &rx_buf, .count = 1 };

    printk("SPI RX Thread started as Slave mode.\n");

    while (1) {
        ret = spi_read(spi_dev, &spi_cfg, &rx_bufs);
        
        if (ret == 0) {
            // 解析数据帧并封装成 ecg_sample_t
            ecg_sample_t sample;
            sample.ecg_data = (int16_t)((rx_raw_bytes[3] << 8) | rx_raw_bytes[2]); // data_h << 8 | data_l
            sample.seq_num = rx_raw_bytes[0]; // id 作为序列号
            sample.timestamp_ms = k_uptime_get_32();
            sample.status = rx_raw_bytes[4]; // type

            // 更新统计信息 
            spi_stats.frames_received++;
            
            /* 详细打印前20个包，确认数据正确性 */
            if (spi_stats.frames_received <= 20 || spi_stats.frames_received % 100 == 0) {
                printk("RX OK (%u): [%02x %02x %02x %02x %02x]\n", 
                       spi_stats.frames_received,
                       rx_raw_bytes[0], rx_raw_bytes[1], rx_raw_bytes[2], 
                       rx_raw_bytes[3], rx_raw_bytes[4]);
            }

            /* 暂时不压入Buffer，直接丢弃，以测试单纯的接收稳定性 */
            /* 
            if (p_ecg_buf != NULL) { ... }
            */

        } else {
            spi_stats.crc_errors++;
            
            /* 错误时打印详细错误码 */
            if (spi_stats.crc_errors <= 20 || spi_stats.crc_errors % 100 == 0) {
                printk("SPI ERR (%d) TotalErr=%u\n", ret, spi_stats.crc_errors);
            }
            
            k_msleep(10); 
        }
    }
}

int spi_comm_init_async(ecg_buffer_t *buffer) {
    spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi2));
    if (!device_is_ready(spi_dev)) {
        printk("Error: SPI device not ready\n");
        return -ENODEV;
    }
    
    spi_cfg.operation = SPI_WORD_SET(8) | SPI_OP_MODE_SLAVE | SPI_TRANSFER_MSB;
    spi_cfg.frequency = 4000000; 
    spi_cfg.slave = 0;          
    
    printk("SPI Slave initialized manually (Success)\n");

    // p_ecg_buf = buffer; // 调试模式未使用

    k_thread_create(&spi_rx_thread_data, spi_rx_thread_stack,
                    K_THREAD_STACK_SIZEOF(spi_rx_thread_stack),
                    spi_rx_thread_entry, NULL, NULL, NULL,
                    SPI_RX_THREAD_PRIORITY, 0, K_NO_WAIT);
    
    return 0;
}

spi_stats_t spi_comm_get_stats(void) {
    return spi_stats;
}