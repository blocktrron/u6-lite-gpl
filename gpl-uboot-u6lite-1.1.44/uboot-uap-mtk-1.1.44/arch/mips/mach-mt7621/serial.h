/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _MACH_MT7621_SERIAL_H_
#define _MACH_MT7621_SERIAL_H_

extern int early_serial_init(void);

#ifdef CONFIG_DEBUG_UART_NS16550
extern void early_serial_printf(const char *fmt, ...);
#else
static inline void early_serial_printf(const char *fmt, ...) {}
#endif

#endif /* _MACH_MT7621_SERIAL_H_ */
