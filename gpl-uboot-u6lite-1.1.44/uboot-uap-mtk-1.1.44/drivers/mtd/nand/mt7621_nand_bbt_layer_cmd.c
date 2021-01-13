/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/errno.h>
#include <linux/sizes.h>
#include <linux/iopoll.h>
#include <jffs2/jffs2.h>
#include <asm/io.h>
#include <console.h>
#include <nand.h>

#include "mt7621_nand.h"

extern struct mtd_info *mbl_mtd_upper_to_lower(struct mtd_info *mtd);
extern u32 mbl_get_bbt_numblk(struct mtd_info *mtd);
extern int mbl_get_bbt_blk_bad(struct mtd_info *mtd, u32 blockidx);
extern u32 mbl_get_bmt(struct mtd_info *mtd, s32 **bmt);
extern void mbl_markbad_update(struct mtd_info *mtd, loff_t offs);
extern void mbl_rescan_bbt_bmt(struct mtd_info *mtd);
extern int mbl_wb_bbt(struct mtd_info *mtd);

static int set_dev(int dev)
{
	struct mtd_info *mtd = get_nand_dev_by_index(dev);

	if (!mtd)
		return -ENODEV;

	if (nand_curr_device == dev)
		return 0;

	printf("Device %d: %s", dev, mtd->name);
	puts("... is now current device\n");
	nand_curr_device = dev;

#ifdef CONFIG_SYS_NAND_SELECT_DEVICE
	board_nand_select_device(mtd_to_nand(mtd), dev);
#endif

	return 0;
}

static int nand_dump(struct mtd_info *mtd, ulong off, int only_oob,
	int repeat)
{
	int i;
	u_char *datbuf, *oobbuf, *p;
	static loff_t last;
	int ret = 0;

	if (repeat)
		off = last + mtd->writesize;

	last = off;

	datbuf = memalign(ARCH_DMA_MINALIGN, mtd->writesize);
	if (!datbuf)
	{
		puts("No memory for page buffer\n");
		return 1;
	}

	oobbuf = memalign(ARCH_DMA_MINALIGN, mtd->oobsize);
	if (!oobbuf)
	{
		puts("No memory for page buffer\n");
		ret = 1;
		goto free_dat;
	}
	off &= ~(mtd->writesize - 1);
	loff_t addr = (loff_t) off;
	struct mtd_oob_ops ops;
	memset(&ops, 0, sizeof(ops));
	ops.datbuf = datbuf;
	ops.oobbuf = oobbuf;
	ops.len = mtd->writesize;
	ops.ooblen = mtd->oobsize;
	ops.mode = MTD_OPS_RAW;
	i = mtd_read_oob(mtd, addr, &ops);
	if (i < 0)
	{
		printf("Error (%d) reading page %08lx\n", i, off);
		ret = 1;
		goto free_all;
	}
	printf("Page %08lx dump:\n", off);

	if (!only_oob)
	{
		i = mtd->writesize >> 4;
		p = datbuf;

		while (i--)
		{
			printf("\t%02x %02x %02x %02x %02x %02x %02x %02x"
				"  %02x %02x %02x %02x %02x %02x %02x %02x\n",
				p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
				p[8], p[9], p[10], p[11], p[12], p[13], p[14],
				p[15]);
			p += 16;
		}
	}

	puts("OOB:\n");
	i = mtd->oobsize >> 3;
	p = oobbuf;
	while (i--)
	{
		printf("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
		p += 8;
	}

free_all:
	free(oobbuf);
free_dat:
	free(datbuf);

	return ret;
}

static int raw_access(struct mtd_info *mtd, ulong addr, loff_t off,
	ulong count, int read, int no_verify)
{
	int ret = 0;

	while (count--) {
		/* Raw access */
		mtd_oob_ops_t ops =
		{
			.datbuf = (u8 *) addr,
			.oobbuf = ((u8 *) addr) + mtd->writesize,
			.len = mtd->writesize,
			.ooblen = mtd->oobsize,
			.mode = MTD_OPS_RAW
		};

		if (read)
		{
			ret = mtd_read_oob(mtd, off, &ops);
		}
		else
		{
			ret = mtd_write_oob(mtd, off, &ops);
			if (!ret && !no_verify)
				ret = nand_verify_page_oob(mtd, &ops, off);
		}

		if (ret)
		{
			printf("%s: error at offset %llx, ret %d\n",
				__func__, (long long) off, ret);
			break;
		}

		addr += mtd->writesize + mtd->oobsize;
		off += mtd->writesize;
	}

	return ret;
}

static void adjust_size_for_badblocks(loff_t *size, loff_t offset, int dev)
{
	/* We grab the nand info object here fresh because this is usually
	* called after arg_off_size() which can change the value of dev.
	*/
	struct mtd_info *mtd = get_nand_dev_by_index(dev);
	loff_t maxoffset = offset + *size;
	int badblocks = 0;

	/* count badblocks in NAND from offset to offset + size */
	for (; offset < maxoffset; offset += mtd->erasesize)
	{
		if (nand_block_isbad(mtd, offset))
			badblocks++;
	}
	/* adjust size if any bad blocks found */
	if (badblocks)
	{
		*size -= badblocks * mtd->erasesize;
		printf("size adjusted to 0x%llx (%d bad blocks)\n",
			(unsigned long long)*size, badblocks);
	}
}

static int do_mbl(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret = 0;
	ulong addr;
	loff_t off, size, maxsize;
	char *cmd, *s;
	struct mtd_info *mtd, *upper_mtd;
	int quiet = 0;
	int dev = nand_curr_device;
	int repeat = flag & CMD_FLAG_REPEAT;

	/* at least two arguments please */
	if (argc < 2)
		goto usage;

	cmd = argv[1];

	/* Only "dump" is repeatable. */
	if (repeat && strcmp(cmd, "dump"))
		return 0;

	if (strcmp(cmd, "info") == 0)
		return run_command("nand info", 0);

	if (strcmp(cmd, "device") == 0)
	{
		if (argc < 3)
		{
			putc('\n');
			if (dev < 0 || dev >= CONFIG_SYS_MAX_NAND_DEVICE)
				puts("no devices available\n");
			else
				run_command("nand info", 0);
			return 0;
		}

		dev = (int) simple_strtoul(argv[2], NULL, 10);
		set_dev(dev);

		return 0;
	}

	/* The following commands operate on the current device, unless
	* overridden by a partition specifier.  Note that if somehow the
	* current device is invalid, it will have to be changed to a valid
	* one before these commands can run, even if a partition specifier
	* for another device is to be used.
	*/
	mtd = get_nand_dev_by_index(dev);
	if (!mtd)
	{
		puts("\nno devices available\n");
		return 1;
	}

	/* Convert MBL upper MTD to original bottom MTD */
	upper_mtd = mtd;
	mtd = mbl_mtd_upper_to_lower(mtd);

	if (strcmp(cmd, "bad") == 0)
	{
		printf("\nDevice %d bad blocks:\n", dev);
		for (off = 0; off < mtd->size; off += mtd->erasesize)
			if (nand_block_isbad(mtd, off))
				printf("  %08llx\n", (unsigned long long)off);
		return 0;
	}

	/*
	* Syntax is:
	*   0    1     2       3    4
	*   nand erase [clean] [off size]
	*/
	if (strncmp(cmd, "erase", 5) == 0 || strncmp(cmd, "scrub", 5) == 0)
	{
		nand_erase_options_t opts;
		/* "clean" at index 2 means request to write cleanmarker */
		int clean = argc > 2 && !strcmp("clean", argv[2]);
		int scrub_yes = argc > 2 && !strcmp("-y", argv[2]);
		int o = (clean || scrub_yes) ? 3 : 2;
		int scrub = !strncmp(cmd, "scrub", 5);
		int spread = 0;
		int args = 2;
		const char *scrub_warn =
			"Warning: "
			"scrub option will erase all factory set bad blocks!\n"
			"         "
			"There is no reliable way to recover them.\n"
			"         "
			"Use this command only for testing purposes if you\n"
			"         "
			"are sure of what you are doing!\n"
			"\nReally scrub this NAND flash? <y/N>\n";

		if (cmd[5] != 0)
		{
			if (!strcmp(&cmd[5], ".spread"))
				spread = 1;
			else if (!strcmp(&cmd[5], ".part"))
				args = 1;
			else if (!strcmp(&cmd[5], ".chip"))
				args = 0;
			else
				goto usage;
		}

		/*
		* Don't allow missing arguments to cause full chip/partition
		* erases -- easy to do accidentally, e.g. with a misspelled
		* variable name.
		*/
		if (argc != o + args)
			goto usage;

		printf("\nNAND %s: ", cmd);
		/* skip first two or three arguments, look for offset and size */
		if (mtd_arg_off_size(argc - o, argv + o, &dev, &off, &size,
			&maxsize, MTD_DEV_TYPE_NAND,
			mtd->size) != 0)
			return 1;

		if (set_dev(dev))
			return 1;

		mtd = get_nand_dev_by_index(dev);

		/* Convert MBL upper MTD to original bottom MTD */
		upper_mtd = mtd;
		mtd = mbl_mtd_upper_to_lower(mtd);

		memset(&opts, 0, sizeof(opts));
		opts.offset = off;
		opts.length = size;
		opts.jffs2 = clean;
		opts.quiet = quiet;
		opts.spread = spread;

		if (scrub)
		{
			if (scrub_yes)
			{
				opts.scrub = 1;
			}
			else
			{
				puts(scrub_warn);
				if (confirm_yesno())
				{
					opts.scrub = 1;
				}
				else
				{
					puts("scrub aborted\n");
					return 1;
				}
			}
		}
		ret = nand_erase_opts(mtd, &opts);
		printf("%s\n", ret ? "ERROR" : "OK");

		if (!ret && opts.scrub)
			mbl_rescan_bbt_bmt(upper_mtd);

		return ret == 0 ? 0 : 1;
	}

	if (strncmp(cmd, "dump", 4) == 0)
	{
		if (argc < 3)
			goto usage;

		off = (int) simple_strtoul(argv[2], NULL, 16);
		ret = nand_dump(mtd, off, !strcmp(&cmd[4], ".oob"), repeat);

		return ret == 0 ? 1 : 0;
	}

	if (strncmp(cmd, "read", 4) == 0 || strncmp(cmd, "write", 5) == 0)
	{
		size_t rwsize;
		ulong pagecount = 1;
		int read;
		int raw = 0;
		int no_verify = 0;

		if (argc < 4)
			goto usage;

		addr = (ulong) simple_strtoul(argv[2], NULL, 16);

		read = strncmp(cmd, "read", 4) == 0; /* 1 = read, 0 = write */
		printf("\nNAND %s: ", read ? "read" : "write");

		s = strchr(cmd, '.');

		if (s && !strncmp(s, ".raw", 4))
		{
			raw = 1;

			if (!strcmp(s, ".raw.noverify"))
				no_verify = 1;

			if (mtd_arg_off(argv[3], &dev, &off, &size, &maxsize,
				MTD_DEV_TYPE_NAND,
				mtd->size))
				return 1;

			if (set_dev(dev))
				return 1;

			mtd = get_nand_dev_by_index(dev);

			/* Convert MBL upper MTD to original bottom MTD */
			upper_mtd = mtd;
			mtd = mbl_mtd_upper_to_lower(mtd);

			if (argc > 4 && !str2long(argv[4], &pagecount))
			{
				printf("'%s' is not a number\n", argv[4]);
				return 1;
			}

			if (pagecount * mtd->writesize > size)
			{
				puts("Size exceeds partition or device limit\n");
				return -1;
			}

			rwsize = pagecount * (mtd->writesize + mtd->oobsize);
		}
		else
		{
			if (mtd_arg_off_size(argc - 3, argv + 3, &dev, &off,
				&size, &maxsize,
				MTD_DEV_TYPE_NAND,
				mtd->size) != 0)
				return 1;

			if (set_dev(dev))
				return 1;

			/* size is unspecified */
			if (argc < 5)
				adjust_size_for_badblocks(&size, off, dev);
			rwsize = size;
		}

		mtd = get_nand_dev_by_index(dev);

		/* Convert MBL upper MTD to original bottom MTD */
		upper_mtd = mtd;
		mtd = mbl_mtd_upper_to_lower(mtd);

		if (!s || !strcmp(s, ".jffs2") ||
			!strcmp(s, ".e") || !strcmp(s, ".i"))
		{
			if (read)
				ret = nand_read_skip_bad(mtd, off, &rwsize,
					NULL, maxsize,
					(u_char *) addr);
			else
				ret = nand_write_skip_bad(mtd, off, &rwsize,
					NULL, maxsize,
					(u_char *) addr,
					WITH_WR_VERIFY);
		}
		else if (!strcmp(s, ".oob"))
		{
			/* out-of-band data */
			mtd_oob_ops_t ops =
			{
				.oobbuf = (u8 *) addr,
				.ooblen = rwsize,
				.mode = MTD_OPS_RAW
			};

			if (read)
				ret = mtd_read_oob(mtd, off, &ops);
			else
				ret = mtd_write_oob(mtd, off, &ops);
		}
		else if (raw)
		{
			ret = raw_access(mtd, addr, off, pagecount, read,
				no_verify);
		}
		else
		{
			printf("Unknown nand command suffix '%s'.\n", s);
			return 1;
		}

		printf(" %zu bytes %s: %s\n", rwsize,
			read ? "read" : "written", ret ? "ERROR" : "OK");

		return ret == 0 ? 0 : 1;
	}

	if (strcmp(cmd, "markbad") == 0)
	{
		argc -= 2;
		argv += 2;

		if (argc <= 0)
			goto usage;

		while (argc > 0)
		{
			addr = simple_strtoul(*argv, NULL, 16);

			if (mtd_block_markbad(mtd, addr))
			{
				printf("block 0x%08lx NOT marked "
					"as bad! ERROR %d\n",
					addr, ret);
				ret = 1;
			}
			else
			{
				printf("block 0x%08lx successfully "
					"marked as bad\n",
					addr);
			}
			--argc;
			++argv;
		}

		mbl_markbad_update(upper_mtd, addr);
		return ret;
	}

	if (strcmp(cmd, "bbt") == 0)
	{
		u32 numblk = mbl_get_bbt_numblk(upper_mtd);
		u32 i, num_bb = 0;

		if (argc >= 3 && !strcmp(argv[2], "wb"))
		{
			if (!mbl_wb_bbt(upper_mtd))
				printf("BBT has been successfully written back\n");

			return 0;
		}

		for (i = 0; i < numblk; i++)
		{
			if (mbl_get_bbt_blk_bad(upper_mtd, i))
			{
				putc('B');
				num_bb++;
			}
			else
			{
				putc('-');
			}
		}
		puts("\n");

		if (num_bb)
		{
			puts("\nBad block summary:\n");
			puts("   BLK    Addr\n");
			for (i = 0; i < numblk; i++)
				if (mbl_get_bbt_blk_bad(upper_mtd, i))
					printf("% 6d    %08x\n", i, i << upper_mtd->erasesize_shift);
			puts("\n");
		}

		return 0;
	}

	if (strcmp(cmd, "bmt") == 0)
	{
		s32 *bmt;
		u32 numblk = mbl_get_bmt(upper_mtd, &bmt);
		u32 i;

		for (i = 0; i < numblk; i++)
		{
			printf("   LBN ->    PBN    LA       -> PA\n");
			if (bmt[i] != -1)
				printf("% 6d -> % 6d    %08x -> %08x\n", i, bmt[i], i << upper_mtd->erasesize_shift, bmt[i] << upper_mtd->erasesize_shift);
			else
				printf("% 6d -> % 6d    %08x -> N/A\n", i, bmt[i], i << upper_mtd->erasesize_shift);
		}

		return 0;
	}

usage:
	return CMD_RET_USAGE;
}

#ifdef CONFIG_SYS_LONGHELP
static char mbl_help_text[] =
"info - show available NAND devices\n"
"mbl device [dev] - show or set current device\n"
"mbl read - addr off size\n"
"mbl write - addr off size\n"
"    read/write 'size' bytes starting at offset 'off'\n"
"    to/from memory address 'addr', skipping bad blocks.\n"
"mbl read.raw - addr off|partition [count]\n"
"mbl write.raw[.noverify] - addr off|partition [count]\n"
"    Use read.raw/write.raw to avoid ECC and access the flash as-is.\n"
"mbl erase[.spread] off size - erase 'size' bytes "
"mbl offset 'off'\n"
"    With '.spread', erase enough for given file size, otherwise,\n"
"    'size' includes skipped bad blocks.\n"
"mbl erase.chip - erase entire chip'\n"
"mbl bad - show bad blocks\n"
"mbl dump[.oob] off - dump page\n"
"mbl scrub [-y] off size | scrub.part partition | scrub.chip\n"
"    really clean NAND erasing bad blocks (UNSAFE)\n"
"mbl markbad off [...] - mark bad block(s) at offset (UNSAFE)\n"
"mbl bbt [wb] - display/writeback bad block table\n"
"mbl bmt - display block mapping table\n"
"";
#endif

U_BOOT_CMD(
	mbl, CONFIG_SYS_MAXARGS, 1, do_mbl,
	"MT7621 NAND BBT layer utility", mbl_help_text
);
