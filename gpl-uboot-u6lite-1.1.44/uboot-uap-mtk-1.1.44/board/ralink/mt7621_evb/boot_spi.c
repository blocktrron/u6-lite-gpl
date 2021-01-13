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
#include <dm.h>
#include <dm/device-internal.h>
#include <spi.h>
#include <spi_flash.h>
#include <jffs2/jffs2.h>
#include <mach/mt7621_regs.h>

#ifndef CONFIG_SPL_BUILD
#if defined (CONFIG_MT7621_SPI_MTD_LAYOUT_DEFAULT)
static const char *def_mtdids = "nor0=raspi";
static const char *def_mtdparts = "mtdparts=raspi:192k(u-boot),64k(u-boot-env),64k(factory),-(firmware)";
#define FIRMWARE_OFFSET		0x50000
#elif defined (CONFIG_MT7621_MTD_LAYOUT_CUSTOMIZE)
#define FIRMWARE_OFFSET		CONFIG_MT7621_FIRMWARE_OFFSET_DEFAULT
#endif

void board_mtdparts_default(const char **mtdids, const char **mtdparts)
{
#ifndef CONFIG_MT7621_MTD_LAYOUT_CUSTOMIZE
	*mtdids = def_mtdids;
	*mtdparts = def_mtdparts;
#endif
}

static struct spi_flash *get_sf_dev(void)
{
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;

#ifdef CONFIG_DM_SPI_FLASH
	struct udevice *new, *bus_dev;
	int ret;

	/* In DM mode defaults will be taken from DT */
	speed = 0, mode = 0;

	/* Remove the old device, otherwise probe will just be a nop */
	ret = spi_find_bus_and_cs(bus, cs, &bus_dev, &new);
	if (!ret)
		device_remove(new, DM_REMOVE_NORMAL);

	ret = spi_flash_probe_bus_cs(bus, cs, speed, mode, &new);
	if (ret)
	{
		printf("Failed to initialize SPI flash at %u:%u (error %d)\n",
			bus, cs, ret);
		return NULL;
	}

	return dev_get_uclass_priv(new);
#else
	struct spi_flash *flash = NULL;

	flash = spi_flash_probe(bus, cs, speed, mode);

	if (!flash)
	{
		printf("Failed to initialize SPI flash at %u:%u\n", bus, cs);
		return NULL;
	}

	return flash;
#endif
}

static int do_mtkboardboot(cmd_tbl_t *cmdtp, int flag, int argc,
	char *const argv[])
{
	char cmd[128];
	struct mtd_device *dev;
	struct part_info *part;
	image_header_t hdr;
	struct spi_flash *sf;
	uint32_t fw_off = FIRMWARE_OFFSET;
	uint32_t load_addr;
	u8 pnum;
	int ret;

	ret = mtdparts_init();
	if (!ret)
	{
		ret = find_dev_and_part("firmware", &dev, &pnum, &part);
		if (!ret)
		{
			if (dev->id->type == MTD_DEV_TYPE_NOR)
			{
				if (part->offset)
					fw_off = (uint32_t) part->offset;
			}
		}
	}

	if (fw_off < SZ_4M - sizeof (hdr))
	{
		memcpy(&hdr,
			(void *) CKSEG1ADDR(MT7621_FLASH_MMAP_BASE + fw_off),
			sizeof(hdr));

		if (image_get_magic(&hdr) != IH_MAGIC)
		{
			printf("Error: no uImage found at 0x%08x\n",
				CKSEG1ADDR(MT7621_FLASH_MMAP_BASE + fw_off));
			return CMD_RET_FAILURE;
		}

		if (image_get_size(&hdr) + sizeof(hdr) + fw_off <= SZ_4M)
		{
			sprintf(cmd, "bootm 0x%08x",
				CKSEG1ADDR(MT7621_FLASH_MMAP_BASE + fw_off));

			return run_command(cmd, 0);
		}
	}

	sf = get_sf_dev();
	if (!sf)
		return CMD_RET_FAILURE;

	ret = spi_flash_read(sf, fw_off, sizeof(hdr), &hdr);
	if (ret)
		return CMD_RET_FAILURE;

	if (image_get_magic(&hdr) != IH_MAGIC)
	{
		printf("Error: no uImage found at flash offset 0x%08x\n",
			fw_off);
		return CMD_RET_FAILURE;
	}

	if (CKSEG0ADDR(image_get_load(&hdr)) -
		CKSEG0ADDR(CONFIG_SYS_LOAD_ADDR)
		>= image_get_size(&hdr) + sizeof(hdr))
		load_addr = CONFIG_SYS_LOAD_ADDR;
	else
		load_addr = CKSEG0ADDR(image_get_load(&hdr)) + SZ_16M;

	printf("Reading from flash 0x%x to mem 0x%08x, size 0x%x ... \n",
		fw_off, load_addr, image_get_size(&hdr) + sizeof(hdr));
	ret = spi_flash_read(sf, fw_off,
		image_get_size(&hdr) + sizeof(hdr),
		(void *) load_addr);
	if (ret)
		return CMD_RET_FAILURE;

	sprintf(cmd, "bootm 0x%08x", load_addr);

	return run_command(cmd, 0);
}

U_BOOT_CMD(mtkboardboot, 1, 0, do_mtkboardboot,
	"Boot MTK firmware", ""
);
#endif /* CONFIG_SPL_BUILD */

void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = BOOT_DEVICE_MTK_MMAP;
	spl_boot_list[1] = BOOT_DEVICE_MTK_UART;
}
