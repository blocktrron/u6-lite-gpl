/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* Early serial console for debugging */

#include <config.h>
#include <common.h>
#include <serial.h>
#include <stdarg.h>
#include <asm/io.h>
#include <mach/mt7621_regs.h>
#include <debug_uart.h>

int early_serial_init(void)
{
	void __iomem *base;

	base = (void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE);

	setbits_le32(base + MT7621_SYS_RSTCTL_REG, REG_MAKE_VAL(UART1_RST, 1));
	clrbits_le32(base + MT7621_SYS_RSTCTL_REG, REG_MAKE_MASK(UART1_RST));

	clrbits_le32(base + MT7621_SYS_GPIO_MODE_REG, REG_MAKE_MASK(UART1_MODE));


	setbits_le32(base + MT7621_SYS_RSTCTL_REG, REG_MAKE_VAL(UART3_RST, 1));
	clrbits_le32(base + MT7621_SYS_RSTCTL_REG, REG_MAKE_MASK(UART3_RST));

	clrbits_le32(base + MT7621_SYS_GPIO_MODE_REG, REG_MAKE_MASK(UART3_MODE));



#ifdef CONFIG_DEBUG_UART_NS16550
	debug_uart_init();
#endif

	return 0;
}

#ifdef CONFIG_DEBUG_UART_NS16550
void early_serial_printf(const char *fmt, ...)
{
	va_list args;
	char printbuffer[1024];

	va_start(args, fmt);

	/* For this to work, printbuffer must be larger than
	* anything we ever want to print.
	*/
	vsprintf(printbuffer, fmt, args);
	va_end(args);

	/* Print the string */
	printascii(printbuffer);
}
#endif
