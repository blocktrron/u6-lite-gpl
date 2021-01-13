/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <nand.h>
#include <malloc.h>

#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/sizes.h>

#include <mach/mt7621_regs.h>

#include "mt7621_nand.h"

static mt7621_nfc_t nfc_dev;
static int nand_valid;

static inline u8 *data_ptr(struct nand_chip *chip, const u8 *buf, int sector_index)
{
	return (u8 *) buf + sector_index * chip->ecc.size;
}

#if 0
static int nand_id_has_period(const u8 *id, int arrlen, int period)
{
	int i, j;
	for (i = 0; i < period; i++)
		for (j = i + period; j < arrlen; j += period)
			if (id[i] != id[j])
				return 0;
	return 1;
}

static int nand_id_len(const u8 *id, int arrlen)
{
	int last_nonzero, period;

	/* Find last non-zero byte */
	for (last_nonzero = arrlen - 1; last_nonzero >= 0; last_nonzero--)
		if (id[last_nonzero])
			break;

	/* All zeros */
	if (last_nonzero < 0)
		return 0;

	/* Calculate wraparound period */
	for (period = 1; period < arrlen; period++)
		if (nand_id_has_period(id, arrlen, period))
			break;

	/* There's a repeated pattern */
	if (period < arrlen)
		return period;

	/* There are trailing zeros */
	if (last_nonzero < arrlen - 1)
		return last_nonzero + 1;

	/* No pattern detected */
	return arrlen;
}

static int nand_get_bits_per_cell(const u8 cellinfo)
{
	int bits;

	bits = cellinfo & NAND_CI_CELLTYPE_MSK;
	bits >>= NAND_CI_CELLTYPE_SHIFT;
	return bits + 1;
}

static void nand_decode_ext_id(mt7621_nfc_t *nfc, const u8 *id)
{
	struct mtd_info *mtd = nand_to_mtd(&nfc->sels[0].nand);
	int extid, id_len;

	/* The 3rd id byte holds MLC / multichip data */
	nfc->sels[0].nand.bits_per_cell = nand_get_bits_per_cell(id[2]);

	/* The 4th id byte is the important one */
	extid = id[3];

	id_len = nand_id_len(id, 8);

	/*
	* Field definitions are in the following datasheets:
	* Old style (4,5 byte ID): Samsung K9GAG08U0M (p.32)
	* New Samsung (6 byte ID): Samsung K9GAG08U0F (p.44)
	* Hynix MLC   (6 byte ID): Hynix H27UBG8T2B (p.22)
	*
	* Check for ID length, non-zero 6th byte, cell type, and Hynix/Samsung
	* ID to decide what to do.
	*/
	if (id_len == 6 && id[0] == NAND_MFR_SAMSUNG &&
		!nand_is_slc(&nfc->sels[0].nand) && id[5] != 0x00)
	{
		/* Calc pagesize */
		mtd->writesize = 2048 << (extid & 0x03);
		extid >>= 2;
		/* Calc oobsize */
		switch (((extid >> 2) & 0x04) | (extid & 0x03))
		{
		case 1:
			mtd->oobsize = 128;
			break;
		case 2:
			mtd->oobsize = 218;
			break;
		case 3:
			mtd->oobsize = 400;
			break;
		case 4:
			mtd->oobsize = 436;
			break;
		case 5:
			mtd->oobsize = 512;
			break;
		case 6:
			mtd->oobsize = 640;
			break;
		case 7:
		default: /* Other cases are "reserved" (unknown) */
			mtd->oobsize = 1024;
			break;
		}
		extid >>= 2;
		/* Calc blocksize */
		mtd->erasesize = (128 * 1024) <<
			(((extid >> 1) & 0x04) | (extid & 0x03));
	}
	else if (id_len == 6 && id[0] == NAND_MFR_HYNIX &&
		!nand_is_slc(&nfc->sels[0].nand))
	{
		unsigned int tmp;

		/* Calc pagesize */
		mtd->writesize = 2048 << (extid & 0x03);
		extid >>= 2;
		/* Calc oobsize */
		switch (((extid >> 2) & 0x04) | (extid & 0x03)) {
		case 0:
			mtd->oobsize = 128;
			break;
		case 1:
			mtd->oobsize = 224;
			break;
		case 2:
			mtd->oobsize = 448;
			break;
		case 3:
			mtd->oobsize = 64;
			break;
		case 4:
			mtd->oobsize = 32;
			break;
		case 5:
			mtd->oobsize = 16;
			break;
		default:
			mtd->oobsize = 640;
			break;
		}
		extid >>= 2;
		/* Calc blocksize */
		tmp = ((extid >> 1) & 0x04) | (extid & 0x03);
		if (tmp < 0x03)
			mtd->erasesize = (128 * 1024) << tmp;
		else if (tmp == 0x03)
			mtd->erasesize = 768 * 1024;
		else
			mtd->erasesize = (64 * 1024) << tmp;
	}
	else
	{
		/* Calc pagesize */
		mtd->writesize = 1024 << (extid & 0x03);
		extid >>= 2;
		/* Calc oobsize */
		mtd->oobsize = (8 << (extid & 0x01)) *
			(mtd->writesize >> 9);
		extid >>= 2;
		/* Calc blocksize. Blocksize is multiples of 64KiB */
		mtd->erasesize = (64 * 1024) << (extid & 0x03);

		/*
		* Toshiba 24nm raw SLC (i.e., not BENAND) have 32B OOB per
		* 512B page. For Toshiba SLC, we decode the 5th/6th byte as
		* follows:
		* - ID byte 6, bits[2:0]: 100b -> 43nm, 101b -> 32nm,
		*                         110b -> 24nm
		* - ID byte 5, bit[7]:    1 -> BENAND, 0 -> raw SLC
		*/
		if (id_len >= 6 && id[0] == NAND_MFR_TOSHIBA &&
			nand_is_slc(&nfc->sels[0].nand) &&
			(id[5] & 0x7) == 0x6 /* 24nm */ &&
			!(id[4] & 0x80) /* !BENAND */)
		{
			mtd->oobsize = 32 * mtd->writesize >> 9;
		}

	}
}

/*
* Old devices have chip data hardcoded in the device ID table. nand_decode_id
* decodes a matching ID table entry and assigns the MTD size parameters for
* the chip.
*/
static void nand_decode_id(mt7621_nfc_t *nfc, struct nand_flash_dev *type, const u8 *id)
{
	struct mtd_info *mtd = nand_to_mtd(&nfc->sels[0].nand);
	int maf_id = id[0];

	mtd->erasesize = type->erasesize;
	mtd->writesize = type->pagesize;
	mtd->oobsize = mtd->writesize / 32;

	/*
	* Check for Spansion/AMD ID + repeating 5th, 6th byte since
	* some Spansion chips have erasesize that conflicts with size
	* listed in nand_ids table.
	* Data sheet (5 byte ID): Spansion S30ML-P ORNAND (p.39)
	*/
	if (maf_id == NAND_MFR_AMD && id[4] != 0x00 && id[5] == 0x00
		&& id[6] == 0x00 && id[7] == 0x00
		&& mtd->writesize == 512) {
		mtd->erasesize = 128 * 1024;
		mtd->erasesize <<= ((id[3] & 0x03) << 1);
	}
}
#endif

static int nfc_device_reset(mt7621_nfc_t *nfc)
{
	nfc_issue_cmd(nfc, NAND_CMD_RESET);

	return nfc_wait_status_ready(nfc, BUSY, "reset");
}

static int nfc_pio_read(mt7621_nfc_t *nfc, void *buf, u32 len)
{
	int bytes_read = 0;
	u8 *bptr;
	u32 *lptr;

	if ((size_t) buf % sizeof(u32) || len % sizeof(u32))
	{
		bptr = (u8 *) buf;

		nfi_setbits16(nfc, NFI_CNFG_REG16, BYTE_RW);

		while (bytes_read < len)
		{
			if (nfc_is_pio_ready(nfc))
				return -EIO;

			bptr[bytes_read] = nfi_read32(nfc, NFI_DATAR_REG32) & 0xff;
			bytes_read += 1;
		}
	}
	else
	{
		lptr = (u32 *) buf;

		nfi_clrbits16(nfc, NFI_CNFG_REG16, BYTE_RW);

		while (bytes_read < len)
		{
			if (nfc_is_pio_ready(nfc))
				return -EIO;

			lptr[bytes_read >> 2] = nfi_read32(nfc, NFI_DATAR_REG32);
			bytes_read += sizeof(u32);
		}
	}

	return 0;
}

static int nfc_parse_chip_id(mt7621_nfc_t *nfc)
{
	struct nand_flash_dev *type = nand_flash_ids;
	struct mtd_info *mtd = nand_to_mtd(&nfc->sels[0].nand);
	void __iomem *base = (void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE);
	u32 bs;
	int i, valid = 0;
	u8 id[NAND_MAX_ID_LEN];

	/* Reset NAND chip */
	nfc_device_reset(nfc);

	/* Reset NFI master */
	nfc_hw_reset(nfc);

	nfi_write16(nfc, NFI_CNFG_REG16, READ_MODE | REG_SET_VAL(OP_MODE, OP_SINGLE_READ));

	nfi_write16(nfc, NFI_CON_REG16, 0);

	nfc_issue_cmd(nfc, NAND_CMD_READID);
	nfc_issue_address(nfc, 0, 0, 1, 0);

	nfc_wait_status_ready(nfc, DATAW | DATAR | ADDR | STA_CMD, "idle mode");

	nfi_write16(nfc, NFI_CON_REG16, NFI_SRD);

	nfc_pio_read(nfc, id, NAND_MAX_ID_LEN);

	valid = 0;

	for (i = 1; i < NAND_MAX_ID_LEN; i++)
		if (id[i] != id[0])
			valid = 1;

	if (!valid)
	{
		printf("Error: no valid NAND chip found!\n");
		return -ENODEV;
	}

	nfc_set_def_timing(&nfc->sels[0], id);

	for (; type->name != NULL; type++)
	{
		if (!type->id_len)
			continue;

		if (!strncmp((char *) type->id, (char *) id, type->id_len))
		{
			mtd->writesize = type->pagesize;
			mtd->erasesize = type->erasesize;
			mtd->oobsize = type->oobsize;

			nfc->sels[0].nand.chipsize = (uint64_t) type->chipsize << 20;

			return 0;
		}
// Don't stop looking when id[1] matches. We may find better matches later
// TODO consider splitting the loop to two - first check all known flashes for full match
// continue with partial match _after_ that
/*		else if (id[1] == type->dev_id)
		{
			nfc->sels[0].nand.chipsize = (uint64_t) type->chipsize << 20;

			if (!type->pagesize)
				nand_decode_ext_id(nfc, id);
			else
				nand_decode_id(nfc, type, id);

			return 0;
		}
*/
	}

	/* Use bootstrap settings */
	bs = readl(base + MT7621_SYS_SYSCFG0_REG);

	switch (REG_GET_VAL(CHIP_MODE, bs))
	{
	case 1:
		mtd->writesize = SZ_2K;
		mtd->oobsize = SZ_64;
		break;
	case 10:
		mtd->writesize = SZ_2K;
		mtd->oobsize = SZ_128;
		break;
	case 11:
		mtd->writesize = SZ_4K;
		mtd->oobsize = SZ_128;
		break;
	case 12:
		mtd->writesize = SZ_4K;
		mtd->oobsize = 224;
		break;
	default:
		return -EINVAL;
	}

	mtd->erasesize = mtd->writesize * 64;
	nfc->sels[0].nand.chipsize = SZ_1M;

	return 0;
}

static int nfc_probe(mt7621_nfc_t *nfc)
{
	mt7621_nfc_sel_t *nfc_sel = &nfc->sels[0];
	struct nand_chip *chip = &nfc_sel->nand;
	u32 num_bits;

	/* For SPL build, uses only CS0 for booting */
	nfi_write16(nfc, NFI_CSEL_REG16, 0);

	if (nfc_parse_chip_id(nfc))
	{
		printf("Error: unable to parse NAND chip ID!\n");
		return -ENODEV;
	}

	nfc_init_chip_config(nfc_sel);

	chip->mtd.erasesize_shift = ffs(chip->mtd.erasesize) - 1;
	chip->mtd.erasesize_mask = (1 << chip->mtd.erasesize_shift) - 1;

	chip->mtd.writesize_shift = ffs(chip->mtd.writesize) - 1;
	chip->mtd.writesize_mask = (1 << chip->mtd.writesize_shift) - 1;

	num_bits = ffs(chip->chipsize) - ffs(chip->mtd.writesize);
	nfc_sel->rowaddr_nob = DIV_ROUND_UP(num_bits, 8);

	nfc_sel->page_cache = malloc(chip->mtd.writesize + chip->mtd.oobsize);

	nfi_write32(nfc, NFI_ACCCON_REG32, nfc_sel->acccon_val);

	nfi_write16(nfc, NFI_PAGEFMT_REG16, nfc_sel->pagefmt_val);

	/* set timeout for polling R/B status */
	nfi_write16(nfc, NFI_CNRN_REG16, REG_MASK(CB2R_TIME) | STR_CNRNB);

	nfc_ecc_init(nfc, &chip->ecc);

	nand_valid = 1;

	return 0;
}

static int nfc_read_page(mt7621_nfc_t *nfc, u32 page_addr)
{
	mt7621_nfc_sel_t *nfc_sel = &nfc->sels[0];
	struct nand_chip *chip = &nfc_sel->nand;
	int i, num_err_bits;

	/* Reset NFI master */
	nfc_hw_reset(nfc);

	nfi_write16(nfc, NFI_CNFG_REG16, HW_ECC_EN | AUTO_FMT_EN | READ_MODE | REG_SET_VAL(OP_MODE, OP_READ));

	nfi_write16(nfc, NFI_CON_REG16, REG_SET_VAL(NFI_SEC, chip->ecc.steps));

	nfc_ecc_decoder_start(nfc);

	nfc_issue_cmd(nfc, NAND_CMD_READ0);
	nfc_issue_address(nfc, 0, page_addr, 2, nfc_sel->rowaddr_nob);
	nfc_issue_cmd(nfc, NAND_CMD_READSTART);

	nfc_wait_status_ready(nfc, DATAW | DATAR | ADDR | STA_CMD, "idle mode");

	nfc_wait_status_ready(nfc, BUSY, "read");

	nfi_setbits16(nfc, NFI_CON_REG16, NFI_BRD);

	for (i = 0; i < chip->ecc.steps; i++)
	{
		nfc_pio_read(nfc, data_ptr(chip, nfc_sel->page_cache, i), chip->ecc.size);

		if (nfc_ecc_decoder_wait_done(nfc, i))
			continue;

		num_err_bits = nfc_ecc_correct_check(nfc, nfc_sel, data_ptr(chip, nfc_sel->page_cache, i), i);

		if (num_err_bits < 0)
			printf("Error: Unrecoverable error bits found in page %d\n", page_addr);
	}

	nfi_clrbits16(nfc, NFI_CON_REG16, NFI_BRD);

	nfc_ecc_decoder_stop(nfc);

	return 0;
}

static int nfc_page_has_badmark(mt7621_nfc_t *nfc, u32 page_addr)
{
	mt7621_nfc_sel_t *nfc_sel = &nfc->sels[0];
	struct nand_chip *chip = &nfc_sel->nand;
	u32 bbmpos;
	u8 bbm;

	if (chip->mtd.writesize == SZ_512)
		bbmpos = 5;
	else
		bbmpos = 0;

	/* Reset NFI master */
	nfc_hw_reset(nfc);

	nfi_write16(nfc, NFI_CNFG_REG16, READ_MODE | REG_SET_VAL(OP_MODE, OP_SINGLE_READ));

	nfi_write16(nfc, NFI_CON_REG16, 0);

	nfc_issue_cmd(nfc, NAND_CMD_READ0);
	nfc_issue_address(nfc, chip->ecc.size + bbmpos, page_addr, 2, nfc_sel->rowaddr_nob);
	nfc_issue_cmd(nfc, NAND_CMD_READSTART);

	nfc_wait_status_ready(nfc, DATAW | DATAR | ADDR | STA_CMD, "idle mode");

	nfc_wait_status_ready(nfc, BUSY, "read");

	nfi_setbits16(nfc, NFI_CON_REG16, NFI_SRD);

	nfc_pio_read(nfc, &bbm, 1);

	nfi_write16(nfc, NFI_CON_REG16, 0);

	return bbm != 0xff;
}

static u32 nfc_lb_to_pb(mt7621_nfc_t *nfc, u32 lb)
{
	mt7621_nfc_sel_t *nfc_sel = &nfc->sels[0];
	struct mtd_info *mtd = &nfc_sel->nand.mtd;
	u32 pb = 0, tlb = 0, num_blocks;

	num_blocks = nfc_sel->nand.chipsize >> mtd->erasesize_shift;

	while (pb < num_blocks)
	{
		if (nfc_page_has_badmark(nfc, (pb << mtd->erasesize_shift) >> mtd->writesize_shift))
		{
			printf("Warning: bad block %d at (%08x)\n", pb, pb << mtd->erasesize_shift);
		}
		else
		{
			if (tlb >= lb)
				return pb;

			tlb++;
		}

		pb++;
	}

	printf("Error: no usable block at %08x\n", lb << mtd->erasesize_shift);
	return (u32) -1UL;
}

int nand_spl_load_image(uint32_t offs, unsigned int size, void *dest)
{
	mt7621_nfc_sel_t *nfc_sel = &nfc_dev.sels[0];
	struct mtd_info *mtd = &nfc_sel->nand.mtd;
	u32 page_addr, col_addr, size_to_read;
	u8 *ptr = (u8 *) dest;
	uint32_t pb, blk_offs;

	if (!nand_valid)
		return -ENODEV;

	blk_offs = offs & mtd->erasesize_mask;
	pb = nfc_lb_to_pb(&nfc_dev, offs >> mtd->erasesize_shift);
	if (pb == (u32) -1UL)
		return -1;

	offs = (pb << mtd->erasesize_shift) | blk_offs;

	while (size)
	{
		if (offs >= nfc_sel->nand.chipsize)
		{
			printf("Error: no usable block at %08x\n", offs >> mtd->erasesize_shift);
			return -1;
		}

		page_addr = offs >> mtd->writesize_shift;
		col_addr = offs & mtd->writesize_mask;
		size_to_read = mtd->writesize - col_addr;
		if (size < size_to_read)
			size_to_read = size;

		if (!nfc_page_has_badmark(&nfc_dev, page_addr))
		{
			nfc_read_page(&nfc_dev, page_addr);
			memcpy(ptr, (u8 *) nfc_sel->page_cache + col_addr, size_to_read);
			ptr += size_to_read;
			size -= size_to_read;
		}
		else
		{
			printf("Warning: bad block %d at (%08x)\n", offs >> mtd->erasesize_shift, offs);
		}

		offs += size_to_read;
	}

	return 0;
}

unsigned long nand_size(void)
{
	if (nand_valid)
		return nfc_dev.sels[0].nand.chipsize;

	return 0;
}

void nand_init(void)
{
	static int initialized;

	if (initialized)
		return;

	initialized = 1;

	nfc_dev.nfi_base = (void __iomem *) CKSEG1ADDR(MT7621_NFI_BASE);
	nfc_dev.ecc_base = (void __iomem *) CKSEG1ADDR(MT7621_NFI_ECC_BASE);
	nfc_dev.sels[0].acccon_val = NFI_DEFAULT_ACCESS_TIMING;

	nfc_probe(&nfc_dev);
}
