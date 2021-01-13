/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef	_MT7621_ASM_SPL_H_
#define	_MT7621_ASM_SPL_H_

#ifndef __ASSEMBLY__
enum {
	BOOT_DEVICE_MTK_MMAP,
	BOOT_DEVICE_MTK_NAND,
	BOOT_DEVICE_MTK_UART,
	BOOT_DEVICE_NONE
};

struct mtk_spl_rom_cfg
{
	uint32_t magic;
	uint32_t size;
	uint32_t align;
	uint32_t reloc_size;
};

/* Linker symbols. */
extern char __bss_start[], __bss_end[];
#endif /* __ASSEMBLY__ */

#define MTK_ROM_CFG_MAGIC_MT7621	0x31323637

#define MTK_SPL_ROM_CFG_OFFS		0x40

#endif
