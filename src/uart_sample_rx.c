#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <errno.h>

#include "uart_sample_rx.h"

LOG_MODULE_REGISTER(uart_sample_rx, LOG_LEVEL_INF);

#define UART_RX_BUF_SIZE 64

static const struct device *uart_dev;
static ecg_buffer_t *p_ecg_buf;

static uart_sample_rx_stats_t uart_stats;
static uint32_t seq_counter;

static uint8_t rx_buf_a[UART_RX_BUF_SIZE];
static uint8_t rx_buf_b[UART_RX_BUF_SIZE];
static bool use_buf_a = false;

static uint8_t pending_low;
static bool have_low;

static void process_rx_byte(uint8_t byte)
{
    uart_stats.bytes_received++;

    if (!have_low) {
        pending_low = byte;
        have_low = true;
        return;
    }

    ecg_sample_t sample;
    sample.ecg_data = (int16_t)(((uint16_t)byte << 8) | pending_low);
    sample.seq_num = seq_counter++;
    sample.timestamp_ms = k_uptime_get_32();
    sample.status = 0;
    have_low = false;

    uart_stats.last_sample = sample.ecg_data;
    uart_stats.last_seq = sample.seq_num;
    uart_stats.has_sample = 1;

    if (p_ecg_buf != NULL) {
        int push_ret = ecg_buffer_push(p_ecg_buf, &sample);
        if (push_ret == 0) {
            uart_stats.samples_received++;
        } else {
            uart_stats.buffer_overruns++;
        }
    } else {
        uart_stats.samples_received++;
    }
}

static void uart_event_handler(const struct device *dev, struct uart_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    switch (evt->type) {
    case UART_RX_RDY:
        for (size_t i = 0; i < evt->data.rx.len; i++) {
            uint8_t b = evt->data.rx.buf[evt->data.rx.offset + i];
            process_rx_byte(b);
        }
        break;

    case UART_RX_BUF_REQUEST: {
        uint8_t *next_buf = use_buf_a ? rx_buf_a : rx_buf_b;
        use_buf_a = !use_buf_a;
        (void)uart_rx_buf_rsp(dev, next_buf, UART_RX_BUF_SIZE);
        break;
    }

    case UART_RX_STOPPED:
        have_low = false;
        break;

    case UART_RX_DISABLED:
        (void)uart_rx_enable(dev, rx_buf_a, UART_RX_BUF_SIZE, 50);
        break;

    default:
        break;
    }
}

int uart_sample_rx_init_async(ecg_buffer_t *buffer)
{
    p_ecg_buf = buffer;
    seq_counter = 0;
    uart_stats = (uart_sample_rx_stats_t){0};
    have_low = false;

    uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));
    if (!device_is_ready(uart_dev)) {
        printk("UART1 device not ready\n");
        return -ENODEV;
    }


    int ret = uart_callback_set(uart_dev, uart_event_handler, NULL);
    if (ret != 0) {
        printk("uart_callback_set failed: %d\n", ret);
        return ret;
    }

    ret = uart_rx_enable(uart_dev, rx_buf_a, UART_RX_BUF_SIZE, 50);
    if (ret != 0) {
        printk("uart_rx_enable failed: %d\n", ret);
        return ret;
    }

    printk("UART1 RX started, 115200 8N1, format=int16 little-endian stream\n");

    return 0;
}

uart_sample_rx_stats_t uart_sample_rx_get_stats(void)
{
    return uart_stats;
}
