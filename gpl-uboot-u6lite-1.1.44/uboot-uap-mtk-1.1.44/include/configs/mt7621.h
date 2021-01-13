/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#ifndef __ASSEMBLY__
extern unsigned long get_cpu_freq(unsigned long);
#endif

#define CONFIG_SYS_HZ			1000
#define CONFIG_SYS_MHZ			get_cpu_freq(0)
#define CONFIG_SYS_MIPS_TIMER_FREQ	(CONFIG_SYS_MHZ / 2)

#define CONFIG_SYS_MONITOR_BASE		CONFIG_SYS_TEXT_BASE

#define CONFIG_SYS_MALLOC_LEN		0x80000
#define CONFIG_SYS_BOOTPARAMS_LEN	0x20000

#define CONFIG_SYS_SDRAM_BASE		0x80000000
#ifdef CONFIG_UBNT_APP
#define CONFIG_SYS_LOAD_ADDR		0x86000000
#else
#define CONFIG_SYS_LOAD_ADDR		0x80100000
#endif

#define CONFIG_VERY_BIG_RAM
#define CONFIG_MAX_MEM_MAPPED		0x1c000000

#ifndef CONFIG_SKIP_LOWLEVEL_INIT
#define CONFIG_SYS_INIT_RAM_ADDR	0xbe108000
#define CONFIG_SYS_INIT_RAM_SIZE	0x5000
#else
#define CONFIG_SYS_INIT_RAM_ADDR	CONFIG_SYS_SDRAM_BASE
#define CONFIG_SYS_INIT_RAM_SIZE	0x8000
#endif
#define CONFIG_SYS_INIT_SP_ADDR		\
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_RAM_SIZE)

#define CONFIG_SYS_MIPS_CACHE_MODE	CONF_CM_CACHABLE_COW

#ifdef CONFIG_BOARD_UBNT_UF_OLT
#define CONFIG_SYS_BOOTM_LEN		(128 << 20)
#else
#define CONFIG_SYS_BOOTM_LEN		(64 << 20)
#endif

/*
 * SPL
 */
#define CONFIG_SYS_UBOOT_START		CONFIG_SYS_TEXT_BASE

/*
 * Serial Port
 */
#ifndef CONFIG_DM_SERIAL
#define CONFIG_SYS_NS16550_REG_SIZE	-4
#define CONFIG_CONS_INDEX		1
#define CONFIG_SYS_NS16550_COM1		0xbe000c00
#define CONFIG_SYS_NS16550_COM2		0xbe000e00
#define CONFIG_SYS_NS16550_SERIAL
#endif
#define CONFIG_SYS_NS16550_CLK		50000000
#define CONFIG_SYS_NS16550_MEM32
#define CONFIG_SYS_BAUDRATE_TABLE \
	{9600, 19200, 38400, 57600, 115200}

#ifdef CONFIG_MT7621_SPI_BOOT
#ifdef CONFIG_UBNT_APP
#define CONFIG_ENV_SIZE			0x1000
#define CONFIG_ENV_OFFSET		(CONFIG_UBNT_PART_UBOOT_KSIZE * 1024)
#else
#define CONFIG_ENV_SIZE			0x10000
#define CONFIG_ENV_OFFSET		0x30000
#endif
#define CONFIG_ENV_SECT_SIZE		0x10000
#else
#define CONFIG_ENV_SIZE			0x20000
#define CONFIG_ENV_OFFSET		0x80000
#define CONFIG_ENV_SECT_SIZE		0x20000
#endif

/* NAND */
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CONFIG_SYS_NAND_MAX_CHIPS	1
#define CONFIG_SYS_NAND_ONFI_DETECTION

/* Network */
#ifdef CONFIG_UBNT_APP
#define CONFIG_IPADDR			192.168.1.20
#define CONFIG_SERVERIP			192.168.1.1
#define CONFIG_NETMASK			255.255.255.0
#else
#define CONFIG_IPADDR			192.168.1.1
#define CONFIG_SERVERIP			192.168.1.2
#define CONFIG_NETMASK			255.255.255.0
#endif

/* Autoboot */
#ifndef CONFIG_UBNT_APP
#define CONFIG_BOOTCOMMAND		"bootmenu 4"
#endif

#ifdef CONFIG_UBNT_APP
#define CONFIG_EXTRA_ENV_SETTINGS   ""
#else
#define CONFIG_EXTRA_ENV_SETTINGS	\
	"bootmenu_0=Startup system (Default)=mtkboardboot\0" \
	"bootmenu_1=Upgrade firmware via TFTP=mtkupgrade fw tftp\0" \
	"bootmenu_2=Upgrade bootloader via TFTP=mtkupgrade bl tftp\0" \
	"bootmenu_3=Upgrade firmware via Serial (Ymodem-1K)=mtkupgrade fw serial\0" \
	"bootmenu_4=Upgrade bootloader via Serial (Ymodem-1K)=mtkupgrade bl serial\0"
#endif

/* MTD */
#define CONFIG_SYS_MTDPARTS_RUNTIME
#define CONFIG_MTD_DEVICE

#ifdef CONFIG_UBNT_APP
#define PHYS_FLASH_1	(CONFIG_SPL_TEXT_BASE)
#define CFG_FLASH_BASE	PHYS_FLASH_1
#define CFG_FLASH_SECTOR_SIZE	(64 * 1024)
#endif

#ifdef CONFIG_UBNT_APP
#if defined(CONFIG_BOARD_UBNT_UAP)
#define CONFIG_MISC_INIT_R	1
#endif
#endif

#endif  /* __CONFIG_H */
