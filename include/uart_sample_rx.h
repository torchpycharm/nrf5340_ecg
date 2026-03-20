#ifndef UART_SAMPLE_RX_H
#define UART_SAMPLE_RX_H

#include <stdint.h>
#include "ecg_buffer.h"

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

#endif /* UART_SAMPLE_RX_H */
