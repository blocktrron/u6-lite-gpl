/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/addrspace.h>
#include <asm/types.h>
#include <asm/spl.h>
#include <linux/mtd/mtd.h>
#include <linux/sizes.h>
#include <jffs2/jffs2.h>

#ifndef CONFIG_SPL_BUILD
#if defined (CONFIG_MT7621_NAND_MTD_LAYOUT_DEFAULT)
static const char *def_mtdids = "nand0=MT7621-NAND";
static const char *def_mtdparts = "mtdparts=MT7621-NAND:512k(u-boot),512k(u-boot-env),256k(factory),-(firmware)";
#define FIRMWARE_OFFSET		0x140000
#elif defined(CONFIG_MT7621_MTD_LAYOUT_CUSTOMIZE)
#define FIRMWARE_OFFSET		CONFIG_MT7621_FIRMWARE_OFFSET_DEFAULT
#endif

void board_mtdparts_default(const char **mtdids, const char **mtdparts)
{
#ifndef CONFIG_MT7621_MTD_LAYOUT_CUSTOMIZE
	*mtdids = def_mtdids;
	*mtdparts = def_mtdparts;
#endif
}

static int do_mtkboardboot(cmd_tbl_t *cmdtp, int flag, int argc,
	char *const argv[])
{
	char cmd[128];
	const char *ep;

	ep = env_get("autostart");
	if (ep)
		ep = strdup(ep);

	env_set("autostart", "yes");

	run_command("nboot firmware", 0);

	sprintf(cmd, "nboot 0x%08x nand0 0x%08x",
		CONFIG_SYS_SDRAM_BASE + SZ_32M, FIRMWARE_OFFSET);
	run_command(cmd, 0);

	if (ep)
	{
		env_set("autostart", ep);
		free((void *) ep);
	}

	return CMD_RET_FAILURE;
}

U_BOOT_CMD(mtkboardboot, 1, 0, do_mtkboardboot,
	"Boot MTK firmware", ""
);
#endif /* CONFIG_SPL_BUILD */

void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = BOOT_DEVICE_MTK_NAND;
	spl_boot_list[1] = BOOT_DEVICE_MTK_UART;
}