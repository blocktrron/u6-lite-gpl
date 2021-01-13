/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <linux/types.h>
#include <debug_uart.h>

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{

}
#endif

#ifdef CONFIG_BOARD_EARLY_INIT_F
int board_early_init_f(void)
{
	return 0;
}
#endif

#ifdef CONFIG_MISC_INIT_R
int misc_init_r(void)
{
	//Force I2C driver to do probe, Even no slave defined in dts
	//this is required for device supported LED bar
	struct udevice *bus;
	if (uclass_get_device_by_name(UCLASS_I2C, "i2c0@1e00900", &bus)) {
		puts("Cannot find I2C bus!\n");
		return 0;
	}
	return 0;
}
#endif
