#ifndef UART_SAMPLE_RX_H
#define UART_SAMPLE_RX_H

#include <stdbool.h>
#include <stdint.h>
#include "ecg_buffer.h"

#define UART_SAMPLE_FRAME_TYPE_DATASET 0xA1
#define UART_SAMPLE_MAX_WINDOW_SAMPLES 9000

typedef struct {
    uint8_t frame_type;
    uint16_t sample_count;
    uint16_t file_id;
    uint32_t rx_timestamp_ms;
} uart_window_header_t;

typedef struct {
    uint32_t bytes_received;
    uint32_t samples_received;
    uint32_t parse_errors;
    uint32_t buffer_overruns;
    int16_t last_sample;
    uint32_t last_seq;
    uint8_t has_sample;
} uart_sample_rx_stats_t;

int uart_sample_rx_init_async(ecg_buffer_t *buffer);
uart_sample_rx_stats_t uart_sample_rx_get_stats(void);
bool uart_sample_rx_try_get_window_header(uart_window_header_t *header);

#endif /* UART_SAMPLE_RX_H */
