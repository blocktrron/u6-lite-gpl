/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <cli.h>
#include <dm.h>
#include <xyzModem.h>
#include <nand.h>
#include <spi.h>
#include <spi_flash.h>
#include <dm/device-internal.h>

#include <asm/reboot.h>
#include <linux/mtd/mtd.h>
#include <linux/sizes.h>
#include <jffs2/jffs2.h>

#define BUF_SIZE 1024

#define COLOR_PROMPT	"\x1b[0;33m"
#define COLOR_INPUT	"\x1b[4;36m"
#define COLOR_ERROR	"\x1b[93;41m"
#define COLOR_CAUTION	"\x1b[1;31m"
#define COLOR_NORMAL	"\x1b[0m"

#if defined (CONFIG_MT7621_NAND_BOOT)
#define MT_MTD_DEV_TYPE		MTD_DEV_TYPE_NAND
#else
#define MT_MTD_DEV_TYPE		MTD_DEV_TYPE_NOR
#endif

enum file_type
{
	TYPE_BL,
	TYPE_BL_ADV,
	TYPE_FW
};

enum source_type
{
	SOURCE_TFTP,
	SOURCE_YMODEM
};

extern char *get_mtk_stage2_image_ptr(char *data, size_t size);
extern int check_mtk_stock_stage2_info(size_t *stage2_off_min, size_t *stage2_off, size_t *align_size);

static void cli_highlight_input(const char *prompt)
{
	printf(COLOR_INPUT "%s" COLOR_NORMAL " ", prompt);
}

static int env_read_cli_set(const char *varname, const char *defval, const char *prompt)
{
	const char *tmpstr;
	char input_buffer[CONFIG_SYS_CBSIZE + 1];
	char *argv[] = {"env", "set", NULL, NULL, NULL};
	int repeatable;

	tmpstr = env_get(varname);
	if (!tmpstr)
		tmpstr = defval;

	if (strlen(tmpstr) > CONFIG_SYS_CBSIZE)
	{
		strncpy(input_buffer, tmpstr, CONFIG_SYS_CBSIZE);
		input_buffer[CONFIG_SYS_CBSIZE] = 0;
	}
	else
	{
		strcpy(input_buffer, tmpstr);
	}

	cli_highlight_input(prompt);
	if (cli_readline_into_buffer(NULL, input_buffer, 0) == -1)
		return -1;

	argv[2] = (char *) varname;
	argv[3] = input_buffer;

	return cmd_process(0, 4, argv, &repeatable, NULL);
}

static int tftp_env_update(const char *varname, const char *defval, const char *prompt)
{
	while (1)
	{
		switch (env_read_cli_set(varname, defval, prompt))
		{
		case 0:
			return 0;
		case -1:
			printf("\n" COLOR_ERROR "*** Operation Aborted! ***" COLOR_NORMAL "\n");
			return 1;
		}
	}
}

static int load_tftp(size_t addr, uint32_t *data_size)
{
	char filename[CONFIG_SYS_CBSIZE + 1];
	uint32_t size;

	if (tftp_env_update("ipaddr", __stringify(CONFIG_IPADDR), "Input U-Boot's IP address:"))
		return CMD_RET_FAILURE;

	if (tftp_env_update("serverip", __stringify(CONFIG_SERVERIP), "Input TFTP server's IP address:"))
		return CMD_RET_FAILURE;

	if (tftp_env_update("netmask", __stringify(CONFIG_NETMASK), "Input IP netmask:"))
		return CMD_RET_FAILURE;

	filename[0] = 0;

	while (1)
	{
		cli_highlight_input("Input file name:");
		if (cli_readline_into_buffer(NULL, filename, 0) == -1)
		{
			printf("\n" COLOR_ERROR "*** Operation Aborted! ***" COLOR_NORMAL "\n");
			return CMD_RET_FAILURE;
		}

		if (filename[0] && filename[0] != 10 && filename[0] != 13)
			break;
	}

	printf("\n");

	load_addr = addr;
	copy_filename(net_boot_file_name, filename, sizeof(net_boot_file_name));

	size = net_loop(TFTPGET);
	if ((int) size < 0)
	{
		printf("\n" COLOR_ERROR "*** TFTP client failure: %d ***" COLOR_NORMAL "\n", size);
		printf("*** Operation Aborted! ***\n");
		return CMD_RET_FAILURE;
	}

	if (data_size)
		*data_size = size;

	return CMD_RET_SUCCESS;
}

static int getcymodem(void)
{
	if (tstc())
		return (getc());
	return -1;
}

static int load_ymodem(size_t addr, uint32_t *data_size)
{
	connection_info_t info;
	char *buf = (char *) addr;
	size_t size = 0;
	int ret, err;

	printf(COLOR_PROMPT "*** Starting serial Ymoden-1K transmitting ***" COLOR_NORMAL "\n\n");

	info.mode = xyzModem_ymodem;
	ret = xyzModem_stream_open(&info, &err);
	if (ret)
	{
		printf("\n" COLOR_ERROR "*** Ymodem error: %s ***" COLOR_NORMAL "\n", xyzModem_error(err));
		printf("*** Operation Aborted! ***\n");
		return CMD_RET_FAILURE;
	}

	while ((ret = xyzModem_stream_read(buf + size, BUF_SIZE, &err)) > 0)
		size += ret;

	xyzModem_stream_close(&err);
	xyzModem_stream_terminate(false, &getcymodem);

	if (data_size)
		*data_size = size;

	return CMD_RET_SUCCESS;
}

static int load_data(enum source_type st, size_t addr, uint32_t *data_size)
{
	switch (st)
	{
	case SOURCE_TFTP:
		if (load_tftp(addr, data_size))
			return CMD_RET_FAILURE;
		break;

	case SOURCE_YMODEM:
		if (load_ymodem(addr, data_size))
			return CMD_RET_FAILURE;
		break;

	default:
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int get_mtd_part_info(const char *partname, size_t *off, size_t *size)
{
	struct mtd_device *dev;
	struct part_info *part;
	u8 pnum;
	int ret;

	ret = mtdparts_init();
	if (ret)
		return ret;

	ret = find_dev_and_part(partname, &dev, &pnum, &part);
	if (ret)
		return ret;

	if (dev->id->type != MT_MTD_DEV_TYPE)
		return -1;

	*off = part->offset;
	*size = part->size;
	return 0;
}

static int check_mtd_bootloader_size(size_t data_size)
{
	size_t part_off, part_size;
	const char *part_name = NULL;

	if (get_mtd_part_info((part_name = "u-boot"), &part_off, &part_size))
		if (get_mtd_part_info((part_name = "Bootloader"), &part_off, &part_size))
			return CMD_RET_SUCCESS;

	if (part_off == 0 && part_size < data_size)
	{
		printf("\n" COLOR_PROMPT "*** Warning: new bootloader will overwrite mtd partition '%s' ***" COLOR_NORMAL "\n", part_name);
		cli_highlight_input("Continue anyway? (N/y):");
		if (!confirm_yesno())
		{
			printf(COLOR_ERROR "*** Operation Aborted! ***" COLOR_NORMAL "\n");
			return CMD_RET_FAILURE;
		}
	}

	return CMD_RET_SUCCESS;
}

#if defined (CONFIG_MT7621_NAND_BOOT)
static void *get_flash_dev(void)
{
	return get_nand_dev_by_index(0);
}

static size_t get_flash_block_size(void *flashdev)
{
	struct mtd_info *mtd = (struct mtd_info *) flashdev;

	return mtd->erasesize;
}

static int do_flash_erase(void *flashdev, size_t offset, size_t len)
{
	struct mtd_info *mtd = (struct mtd_info *) flashdev;
	struct erase_info instr;
	int ret;

	memset(&instr, 0, sizeof(instr));

	instr.addr = offset;
	instr.len = len;

	ret = mtd_erase(mtd, &instr);
	if (ret)
		return ret;

	if (instr.state != MTD_ERASE_DONE)
		return -1;

	return 0;
}

static int do_nand_verify(struct mtd_info *mtd, u32 offset, size_t len, const void *buf)
{
	u8 data[SZ_4K];
	int ret;
	size_t retlen, readlen;
	const u8 *ptr = (const u8 *) buf;

	while (len)
	{
		readlen = sizeof(data);
		if (readlen > len)
			readlen = len;

		ret = mtd_read(mtd, offset, readlen, &retlen, data);
		if (ret && ret != -EUCLEAN)
			return ret;

		if (readlen != retlen)
			return 1;

		if (memcmp(data, ptr, readlen))
			return 1;

		offset += readlen;
		ptr += readlen;
		len -= readlen;
	}

	return 0;
}

static int do_flash_write(void *flashdev, u32 offset, size_t len, const void *buf)
{
	struct mtd_info *mtd = (struct mtd_info *) flashdev;
	int ret;
	size_t retlen;

	ret = mtd_write(mtd, offset, len, &retlen, buf);
	if (ret && ret != -EUCLEAN)
		return ret;

	return do_nand_verify(mtd, offset, len, buf);
}
#else
static void *get_flash_dev(void)
{
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;
	struct spi_flash *flash = NULL;
#ifdef CONFIG_DM_SPI_FLASH
	struct udevice *new, *bus_dev;
	int ret;
	/* In DM mode defaults will be taken from DT */
	speed = 0, mode = 0;
#else
	struct spi_flash *new;
#endif

#ifdef CONFIG_DM_SPI_FLASH
	/* Remove the old device, otherwise probe will just be a nop */
	ret = spi_find_bus_and_cs(bus, cs, &bus_dev, &new);
	if (!ret)
		device_remove(new, DM_REMOVE_NORMAL);

	ret = spi_flash_probe_bus_cs(bus, cs, speed, mode, &new);
	if (ret) {
		printf("Failed to initialize SPI flash at %u:%u (error %d)\n",
			bus, cs, ret);
		return NULL;
	}

	flash = dev_get_uclass_priv(new);
#else
	if (flash)
		spi_flash_free(flash);

	new = spi_flash_probe(bus, cs, speed, mode);
	flash = new;

	if (!new) {
		printf("Failed to initialize SPI flash at %u:%u\n", bus, cs);
		return NULL;
	}

	flash = new;
#endif

	return flash;
}

static size_t get_flash_block_size(void *flashdev)
{
	struct spi_flash *flash = (struct spi_flash *) flashdev;

	return flash->erase_size;
}

static int do_flash_erase(void *flashdev, size_t offset, size_t len)
{
	struct spi_flash *flash = (struct spi_flash *) flashdev;

	return spi_flash_erase(flash, offset, len);
}

static int do_flash_write(void *flashdev, u32 offset, size_t len, const void *buf)
{
	struct spi_flash *flash = (struct spi_flash *) flashdev;

	return spi_flash_write(flash, offset, len, buf);
}
#endif

static int prompt_countdown(const char *prompt, int delay)
{
	int i;
	int hit = 0;

	if (delay <= 0)
		return 0;

	printf("\n%s: %2d ", prompt, delay);

	while (delay > 0)
	{
		for (i = 0; i < 100; i++)
		{
			if (!tstc())
			{
				mdelay(10);
				continue;
			}

			getc();
			hit = 1;
			delay = -1;

			break;
		}

		if (delay < 0)
			break;

		delay--;
		printf("\b\b\b%2d ", delay);
	}

	puts("\n");

	return hit;
}

static int do_write_bootloader(void *flash, size_t stock_stage2_off, size_t data_addr, uint32_t data_size, uint32_t stage1_size, int adv)
{
	uint32_t erase_size;
	int ret;

	if (stock_stage2_off && stage1_size)
	{
		if (adv)
		{
			printf(COLOR_PROMPT "*** Caution: Bootblock upgrading ***" COLOR_NORMAL "\n\n");
			printf("This bootloader contains Bootblock.\n");
			printf(COLOR_CAUTION "Upgrading Bootblock is very dangerous. Upgrade it only if you know what you are doing!" COLOR_NORMAL "\n");
			cli_highlight_input("Upgrade Bootblock? (N/y):");

			if (!confirm_yesno())
			{
				printf("Only second stage block will be upgraded\n");
				data_addr += stage1_size;
				data_size -= stage1_size;
			}
			else
			{
				printf("Whole bootloader will be upgraded\n");
				stock_stage2_off = 0;
			}
		}
		else
		{
			data_addr += stage1_size;
			data_size -= stage1_size;
		}
	}

	if (check_mtd_bootloader_size(stock_stage2_off + data_size) != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;

	printf("\n");

	erase_size = ALIGN(data_size, get_flash_block_size(flash));

	printf("Erasing from 0x%x to 0x%x, size 0x%x ... ", stock_stage2_off, stock_stage2_off + erase_size - 1, erase_size);

	ret = do_flash_erase(flash, stock_stage2_off, erase_size);

	if (ret)
	{
		printf("Fail\n");
		printf(COLOR_ERROR "*** Flash erasure [%x-%x] failed! ***" COLOR_NORMAL "\n", stock_stage2_off, stock_stage2_off + erase_size - 1);
		return CMD_RET_FAILURE;
	}

	printf("OK\n");

	printf("Writting from 0x%x to 0x%x, size 0x%x ... ", data_addr, stock_stage2_off, data_size);

	ret = do_flash_write(flash, stock_stage2_off, data_size, (void *) data_addr);

	if (ret)
	{
		printf("Fail\n");
		printf(COLOR_ERROR "*** Flash program [%x-%x] failed! ***" COLOR_NORMAL "\n", stock_stage2_off, stock_stage2_off + data_size - 1);
		return CMD_RET_FAILURE;
	}

	printf("OK\n");

	printf("\n" COLOR_PROMPT "*** Bootloader upgrade completed! ***" COLOR_NORMAL "\n");

	if (!prompt_countdown("Hit any key to stop reboot", 3))
	{
		printf("\nRebooting ...\n\n");
		_machine_restart();
	}

	return CMD_RET_SUCCESS;
}

static int verify_stage2_integrity(const void *data, uint32_t size)
{
	struct image_header hdr;
	u32 data_chksum, data_size;

	/* Header checksum has already been validated in get_mtk_stage2_image_ptr */

	memcpy(&hdr, data, sizeof(hdr));

	if (image_get_magic(&hdr) != IH_MAGIC)
		return 1;

	data_chksum = image_get_dcrc(&hdr);
	data_size = image_get_size(&hdr);

	if (data_size + sizeof(struct image_header) > size)
		return 1;

	if (crc32(0, (const u8 *) data + sizeof(struct image_header), data_size) != data_chksum)
		return 1;

	return 0;
}

static int write_bootloader(void *flash, size_t data_addr, uint32_t data_size, int adv)
{
	size_t stock_stage2_off, stock_stage2_off_min, stock_block_size, flash_block_size;
	size_t tmp;
	char *data_stage2_ptr;

	data_stage2_ptr = get_mtk_stage2_image_ptr((char *) data_addr, data_size);
	if (!data_stage2_ptr)
	{
		/* New bootloader is not a two-stage bootloader */
		return do_write_bootloader(flash, 0, data_addr, data_size, 0, adv);
	}

	/* Verify stage2 block */
	if (verify_stage2_integrity(data_stage2_ptr, data_size - (data_addr - (size_t) data_stage2_ptr)))
	{
		printf(COLOR_ERROR "*** Bootloader stage2 block integrity check failed! ***" COLOR_NORMAL "\n");
		return CMD_RET_FAILURE;
	}

	if (!check_mtk_stock_stage2_info(&stock_stage2_off_min, &stock_stage2_off, &stock_block_size))
	{
		/* Current bootloader is not a two-stage bootloader */
		return do_write_bootloader(flash, 0, data_addr, data_size, 0, adv);
	}

	flash_block_size = get_flash_block_size(flash);
	if (!flash_block_size)
	{
		/* Unable to get erase block size */
		return do_write_bootloader(flash, 0, data_addr, data_size, 0, adv);
	}

	if (stock_block_size != flash_block_size)
	{
		/* Current bootloader's alignment is not equal to the real erase block size */
		if (stock_block_size < flash_block_size)
		{
			/* Current bootloader's alignment is smaller than real erase block size */
			if (flash_block_size % stock_block_size)
			{
				/* And can not divide the real erase block size */
				return do_write_bootloader(flash, 0, data_addr, data_size, 0, adv);
			}

			if (stock_stage2_off % flash_block_size)
			{
				/* Current bootloader's stage2 offset is not on the erase block boundary */
				if (stock_stage2_off_min % flash_block_size)
				{
					/* Start from the minimum offset, find a offset aligned with the real erase block size */
					tmp = ALIGN(stock_stage2_off_min, flash_block_size);

					if (tmp > stock_stage2_off)
					{
						/* The new offset exceeds the original stage2 offset */
						return do_write_bootloader(flash, 0, data_addr, data_size, 0, adv);
					}
					else
					{
						/* Use the new offset */
						stock_stage2_off = tmp;
					}
				}
				else
				{
					/* The minimum offset is aligned with the real erase block size */
					stock_stage2_off = stock_stage2_off_min;
				}
			}
		}
		else
		{
			/* Current bootloader's alignment is larger than real erase block size */
			if (stock_block_size % flash_block_size)
			{
				/* And is not a multiple of the real erase block size */
				return do_write_bootloader(flash, 0, data_addr, data_size, 0, adv);
			}
		}
	}

	return do_write_bootloader(flash, stock_stage2_off, data_addr, data_size, (size_t) data_stage2_ptr - data_addr, adv);
}

static int write_firmware(void *flash, size_t data_addr, uint32_t data_size)
{
	uint32_t erase_size;
	size_t part_off, part_size;
	int ret;

	if (get_mtd_part_info("firmware", &part_off, &part_size))
	{
		printf(COLOR_ERROR "*** MTD partition 'firmware' does not exist! ***" COLOR_NORMAL "\n");
		return CMD_RET_FAILURE;
	}

	if (!part_off)
	{
		printf(COLOR_ERROR "*** MTD partition 'firmware' is not valid! ***" COLOR_NORMAL "\n");
		return CMD_RET_FAILURE;
	}

	if (part_off % get_flash_block_size(flash))
	{
		printf(COLOR_ERROR "*** MTD partition 'firmware' does not start on erase boundary! ***" COLOR_NORMAL "\n");
		return CMD_RET_FAILURE;
	}

	if (part_size < data_size)
	{
		printf("\n" COLOR_PROMPT "*** Warning: new firmware will overwrite mtd partition 'firmware' ***" COLOR_NORMAL "\n");
		cli_highlight_input("Continue anyway? (N/y):");
		if (!confirm_yesno())
		{
			printf(COLOR_ERROR "*** Operation Aborted! ***" COLOR_NORMAL "\n");
			return CMD_RET_FAILURE;
		}
	}

	printf("\n");

	erase_size = ALIGN(data_size, get_flash_block_size(flash));

	printf("Erasing from 0x%x to 0x%x, size 0x%x ... ", part_off, part_off + erase_size - 1, erase_size);

	ret = do_flash_erase(flash, part_off, erase_size);

	if (ret)
	{
		printf("Fail\n");
		printf(COLOR_ERROR "*** Flash erasure [%x-%x] failed! ***" COLOR_NORMAL "\n", part_off, part_off + erase_size - 1);
		return CMD_RET_FAILURE;
	}

	printf("OK\n");

	printf("Writting from 0x%x to 0x%x, size 0x%x ... ", data_addr, part_off, data_size);

	ret = do_flash_write(flash, part_off, data_size, (void *) data_addr);

	if (ret)
	{
		printf("Fail\n");
		printf(COLOR_ERROR "*** Flash program [%x-%x] failed! ***" COLOR_NORMAL "\n", part_off, part_off + data_size - 1);
		return CMD_RET_FAILURE;
	}

	printf("OK\n");

	printf("\n" COLOR_PROMPT "*** Firmware upgrade completed! ***" COLOR_NORMAL "\n");

	if (!prompt_countdown("Hit any key to stop firmware bootup", 3))
		run_command("mtkboardboot", 0);

	return CMD_RET_SUCCESS;
}

static int write_data(enum file_type ft, size_t addr, uint32_t data_size)
{
	void *flash;

	flash = get_flash_dev();

	if (!flash)
		return CMD_RET_FAILURE;

	switch (ft)
	{
	case TYPE_BL:
	case TYPE_BL_ADV:
		if (write_bootloader(flash, addr, data_size, (ft == TYPE_BL_ADV)))
			return CMD_RET_FAILURE;

		break;

	case TYPE_FW:
		if (write_firmware(flash, addr, data_size))
			return CMD_RET_FAILURE;

		break;

	default:
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int do_mtkupgrade(cmd_tbl_t *cmdtp, int flag, int argc,
	char *const argv[])
{
	enum file_type ft;
	enum source_type st;
	const char *ft_name, *st_name;

	size_t data_load_addr;
	uint32_t data_size = 0;

	if (argc < 3)
		return CMD_RET_USAGE;

	if (!strcasecmp(argv[1], "bl"))
	{
		ft = TYPE_BL;
		ft_name = "Bootloader";
	}
	else if (!strcasecmp(argv[1], "bladv"))
	{
		ft = TYPE_BL_ADV;
		ft_name = "Bootloader";
	}
	else if (!strcasecmp(argv[1], "fw"))
	{
		ft = TYPE_FW;
		ft_name = "Firmware";
	}
	else
	{
		printf("Error: invalid type '%s'\n", argv[1]);
		return EINVAL;
	}

	if (!strcasecmp(argv[2], "tftp"))
	{
		st = SOURCE_TFTP;
		st_name = "TFTP";
	}
	else if (!strcasecmp(argv[2], "serial"))
	{
		st = SOURCE_YMODEM;
		st_name = "Serial (Ymodem-1K)";
	}
	else
	{
		printf("Error: invalid source '%s'\n", argv[2]);
		return EINVAL;
	}

	printf("\n" COLOR_PROMPT "*** Upgrading %s via %s ***" COLOR_NORMAL "\n\n", ft_name, st_name);

	data_load_addr = CONFIG_SYS_LOAD_ADDR;

	/* Load data */
	if (load_data(st, data_load_addr, &data_size) != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;

	printf("\n" COLOR_PROMPT "*** Loaded %d (0x%x) bytes at 0x%08x ***" COLOR_NORMAL "\n\n", data_size, data_size, data_load_addr);

	/* Write data */
	if (write_data(ft, data_load_addr, data_size) != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(mtkupgrade, 3, 0, do_mtkupgrade,
	"MTK firmware/bootloader upgrading utility",
	"mtkupgrade <type> <source>\n"
	"type    - upgrade file type\n"
	"          bl      - Bootloader\n"
	"          bladv   - Bootloader (Advanced)\n"
	"          fw      - Firmware\n"
	"source  - source for loading file\n"
	"          tftp    - via TFTP client\n"
	"          serial  - via serial Ymodem-1K\n"
);
