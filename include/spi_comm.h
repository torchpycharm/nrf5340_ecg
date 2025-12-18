#ifndef SPI_COMM_H
#define SPI_COMM_H

#include <stdint.h>

#define SPI_FRAME_SIZE 5

typedef struct {
    uint8_t id;
    uint8_t ack;
    uint8_t data_l;
    uint8_t data_h;
    // uint8_t checksum;
    uint8_t type;
} spi_frame_t;

/* SPI 初始化，必须调用一次。 */
int spi_comm_init(void);

/* 从 SPI 接收一帧（阻塞），并写入 frame_out。返回 0 成功，<0 失败。 */
int spi_comm_read_frame(spi_frame_t *frame_out);

#endif /* SPI_COMM_H */
