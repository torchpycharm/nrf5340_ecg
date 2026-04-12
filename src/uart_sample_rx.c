#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <errno.h>

#include "uart_sample_rx.h"

LOG_MODULE_REGISTER(uart_sample_rx, LOG_LEVEL_INF);

#define UART_RX_BUF_SIZE 64
#define UART_FRAME_HEADER_SIZE 5

typedef enum {
    UART_RX_MODE_WAIT_HEADER = 0,
    UART_RX_MODE_STREAM_SAMPLES,
} uart_rx_mode_t;

static const struct device *uart_dev;
static ecg_buffer_t *p_ecg_buf;

static uart_sample_rx_stats_t uart_stats;
static uint32_t seq_counter;

static uint8_t rx_buf_a[UART_RX_BUF_SIZE];
static uint8_t rx_buf_b[UART_RX_BUF_SIZE];
static bool use_buf_a = false;

static uint8_t pending_low;
static bool have_low;
static uart_rx_mode_t rx_mode;
static uint8_t header_buf[UART_FRAME_HEADER_SIZE];
static uint8_t header_pos;
static uint16_t samples_remaining;

static uart_window_header_t latest_header;
static bool header_ready;

static void publish_header(uint8_t frame_type, uint16_t sample_count, uint16_t file_id)
{
    unsigned int key = irq_lock();
    latest_header.frame_type = frame_type;
    latest_header.sample_count = sample_count;
    latest_header.file_id = file_id;
    latest_header.rx_timestamp_ms = k_uptime_get_32();
    header_ready = true;
    irq_unlock(key);
}

static void process_sample_byte(uint8_t byte)
{
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

    if (samples_remaining > 0) {
        samples_remaining--;
        if (samples_remaining == 0) {
            rx_mode = UART_RX_MODE_WAIT_HEADER;
            header_pos = 0;
            have_low = false;
            LOG_INF("UART frame payload complete; waiting next header");
        }
    }
}

static void process_rx_byte(uint8_t byte)
{
    uart_stats.bytes_received++;

    if (rx_mode == UART_RX_MODE_WAIT_HEADER) {
        if (header_pos == 0 && byte != UART_SAMPLE_FRAME_TYPE_DATASET) {
            return;
        }

        header_buf[header_pos++] = byte;
        if (header_pos < UART_FRAME_HEADER_SIZE) {
            return;
        }

        uint8_t frame_type = header_buf[0];
        uint16_t sample_count = (uint16_t)header_buf[1] |
                                ((uint16_t)header_buf[2] << 8);
        uint16_t file_id = (uint16_t)header_buf[3] |
                           ((uint16_t)header_buf[4] << 8);

        header_pos = 0;

        if (frame_type != UART_SAMPLE_FRAME_TYPE_DATASET ||
            sample_count == 0 ||
            sample_count > UART_SAMPLE_MAX_WINDOW_SAMPLES) {
            uart_stats.parse_errors++;
            LOG_WRN("Invalid UART frame header: type=0x%02x count=%u file=%u",
                    frame_type, sample_count, file_id);
            return;
        }

        samples_remaining = sample_count;
        have_low = false;
        rx_mode = UART_RX_MODE_STREAM_SAMPLES;
        publish_header(frame_type, sample_count, file_id);

        LOG_INF("UART frame header received: type=0x%02x file=%u samples=%u",
                frame_type, file_id, sample_count);
        return;
    }

    process_sample_byte(byte);
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
        header_pos = 0;
        samples_remaining = 0;
        rx_mode = UART_RX_MODE_WAIT_HEADER;
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
    rx_mode = UART_RX_MODE_WAIT_HEADER;
    header_pos = 0;
    samples_remaining = 0;
    header_ready = false;
    latest_header = (uart_window_header_t){0};

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

    printk("UART1 RX started, expecting frame header [type(1),count(2),file(2)] + int16 LE payload\n");

    return 0;
}

uart_sample_rx_stats_t uart_sample_rx_get_stats(void)
{
    return uart_stats;
}

bool uart_sample_rx_try_get_window_header(uart_window_header_t *header)
{
    if (header == NULL) {
        return false;
    }

    unsigned int key = irq_lock();
    if (!header_ready) {
        irq_unlock(key);
        return false;
    }

    *header = latest_header;
    header_ready = false;
    irq_unlock(key);
    return true;
}
