#include <zephyr/drivers/spi.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include "spi_comm.h"

LOG_MODULE_REGISTER(spi_comm, LOG_LEVEL_INF);

static const struct device *spi_dev;
static struct spi_config spi_cfg;


int spi_comm_init(void)
{
    spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi2));  
    if (!device_is_ready(spi_dev)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }

    spi_cfg.operation =
        SPI_WORD_SET(8) |
        SPI_OP_MODE_SLAVE |
        SPI_LINES_SINGLE;


    spi_cfg.frequency = 0;  /* 从机频率无用 */
    spi_cfg.slave = 0;

    LOG_INF("SPI slave initialized (8-bit, slave mode)");
    return 0;
}

int spi_comm_read_frame(spi_frame_t *frame_out)
{
    uint8_t buf[SPI_FRAME_SIZE];
    struct spi_buf rx_buf = { .buf = buf, .len = SPI_FRAME_SIZE };
    struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };
    // printk("SPI reading frame...\n");
    int ret = spi_read(spi_dev, &spi_cfg, &rx);
    // printk("frame received.\n");
    if (ret < 0) {
        LOG_ERR("spi_read failed: %d", ret);
        return ret;
    }

    /* 填充结构体 */
    frame_out->id       = buf[0];
    frame_out->ack      = buf[1];
    frame_out->data_l   = buf[2];
    frame_out->data_h   = buf[3];
    // frame_out->checksum = buf[4];
    frame_out->type     = buf[4];

    /* 校验和 (sum of bytes 0,1,2,3,5) */
    // uint8_t sum = buf[0] + buf[1] + buf[2] + buf[3] + buf[5];
    // if (sum != frame_out->checksum) {
    //     LOG_WRN("Checksum mismatch: calc=0x%02X recv=0x%02X",
    //             sum, frame_out->checksum);
    //     return -1;
    // }

    return 0;
}
