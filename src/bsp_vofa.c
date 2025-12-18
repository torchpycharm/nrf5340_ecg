#include "bsp_vofa.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#include <stdint.h>

static const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

int TO_Vofa_DATA(void)
{
	static transform transform_data;
	static uint8_t flag = 1;
	uint32_t tick = k_uptime_get_32(); (void)tick;
	  if(flag == 1){
        transform_data.cdata[NOFCHANEL*4+0]=0x00;
        transform_data.cdata[NOFCHANEL*4+1]=0x00;
        transform_data.cdata[NOFCHANEL*4+2]=0x80;
        transform_data.cdata[NOFCHANEL*4+3]=0x7f;
        flag = 0;
    }
		//transform_data.fdata[0] = (float)Angle_pid.time;

	/* Ensure UART device is ready */
	if (!device_is_ready(uart_dev)) {
		return 0;
	}

	/* Transmit using Zephyr UART API. Use a small timeout in milliseconds to
	 * avoid blocking indefinitely. In this Zephyr/NCS version `uart_tx`
	 * expects an `int32_t` timeout in milliseconds, so pass 200 directly.
	 */
	//此处后续改为中断形式，不要使用该阻塞发送的函数
	if (uart_tx(uart_dev, transform_data.cdata, NOFCHANEL * 4 + 4, 200) == 0) {
		return 1;
	}

	return 0;
}