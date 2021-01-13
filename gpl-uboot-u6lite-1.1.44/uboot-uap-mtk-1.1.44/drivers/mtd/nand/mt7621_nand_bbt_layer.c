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
#include <asm/io.h>


const u8 mtk_bbt_sig[] = "mtknand";
#define MTK_BBT_SIG_OFF			1
#define MTK_BBT_MAX_BLOCK_NUM		32
#define MTK_BBT_BITS_PER_BLOCK		2
#define MTK_BBT_GOOD_BLOCK_MASK		0
#define MTK_BBT_RETRIES			3

struct mtk_bbt_layer
{
	struct mtd_info mtd;		/* new mtd layer */
	struct mtd_info *bott_mtd;	/* original NAND-based mtd */

	u8 *bbt;
	u32 bbt_block;			/* Physical block number */
	u32 bbt_num_blocks;
	u32 bbt_size;
	u32 num_blocks;

	s32 *bmt;			/* Logic block index, physical block number */
	u32 avail_blocks;
};

int mbl_wb_bbt(struct mtd_info *mtd);

static inline struct mtk_bbt_layer *mtd_to_mbl(struct mtd_info *mtd)
{
	return container_of(mtd, struct mtk_bbt_layer, mtd);
}

static void mbl_bbt_mark_bad(struct mtk_bbt_layer *mbl, u32 blockidx)
{
	u32 idx, pos;

	if (blockidx >= mbl->num_blocks)
		return;

	idx = blockidx / (8 / MTK_BBT_BITS_PER_BLOCK);
	pos = blockidx % (8 / MTK_BBT_BITS_PER_BLOCK);

	mbl->bbt[idx] |= ((1 << MTK_BBT_BITS_PER_BLOCK) - 1) << (pos * MTK_BBT_BITS_PER_BLOCK);
}

static int mbl_bbt_is_bad(struct mtk_bbt_layer *mbl, u32 blockidx)
{
	u32 idx, pos;

	if (blockidx >= mbl->num_blocks)
		return -1;

	idx = blockidx / (8 / MTK_BBT_BITS_PER_BLOCK);
	pos = blockidx % (8 / MTK_BBT_BITS_PER_BLOCK);

	return ((mbl->bbt[idx] >> (pos * MTK_BBT_BITS_PER_BLOCK)) & ((1 << MTK_BBT_BITS_PER_BLOCK) - 1)) != MTK_BBT_GOOD_BLOCK_MASK;
}

static void mbl_update_bmt(struct mtk_bbt_layer *mbl)
{
	u32 pb, lb;

	for (pb = 0, lb = 0; pb < mbl->num_blocks; pb++)
	{
		if (mbl_bbt_is_bad(mbl, pb))
			continue;

		mbl->bmt[lb++] = pb;
	}

	mbl->avail_blocks = lb;

	/* Unusable blocks */
	for (pb = lb; pb < mbl->num_blocks; pb++)
		mbl->bmt[pb] = -1;
}

static int mbl_read_bbt_skip_bad(struct mtk_bbt_layer *mbl, u32 blockidx)
{
	u32 size_read = 0, size_to_read;
	loff_t off;
	size_t retlen;
	int ret;

	while (blockidx < mbl->num_blocks)
	{
		if (size_read >= mbl->bbt_size)
			return 0;

		off = blockidx << mbl->bott_mtd->erasesize_shift;

		if (mtd_block_isbad(mbl->bott_mtd, off))
		{
			blockidx++;
			continue;
		}

		size_to_read = mbl->bbt_size - size_read;
		if (mbl->bott_mtd->erasesize < size_to_read)
			size_to_read = mbl->bott_mtd->erasesize;

		ret = mtd_read(mbl->bott_mtd, off, size_to_read, &retlen, mbl->bbt + size_read);
		if ((ret == 0 || ret == -EUCLEAN) && (retlen == size_to_read))
			size_read += size_to_read;
	}

	if (size_read < mbl->bbt_size)
		return 1;

	return 0;
}

static int mbl_init_bbt(struct mtk_bbt_layer *mbl, int *bbtwb)
{
	struct mtd_oob_ops ops;
	u8 sig[sizeof (mtk_bbt_sig) + MTK_BBT_SIG_OFF];
	u32 num_bits, pb;
	loff_t addr;
	int ret, bbt_exist = 0;

	mbl->num_blocks = mbl->bott_mtd->size >> mbl->bott_mtd->erasesize_shift;	/* Number of blocks */
	num_bits = mbl->num_blocks * MTK_BBT_BITS_PER_BLOCK;				/* Number of bits needed for BBT */
	mbl->bbt_size = DIV_ROUND_UP(num_bits, 8);					/* Bits to bytes */

	mbl->bbt_num_blocks = DIV_ROUND_UP(mbl->bbt_size, mbl->bott_mtd->erasesize);

	mbl->bbt = (u8 *) malloc(mbl->bbt_size);
	if (!mbl->bbt)
	{
		printf("mtk_bbt_layer: Unable to allocate %d bytes for BBT\n", mbl->bbt_size);
		return -EINVAL;
	}

	*bbtwb = 0;

	/* Try to read existing bbt */
	memset(&ops, 0, sizeof(ops));
	ops.datbuf = NULL;
	ops.oobbuf = sig;
	ops.len = 0;
	ops.ooblen = sizeof(sig);
	ops.mode = MTD_OPS_RAW;

	for (pb = mbl->num_blocks - mbl->bbt_num_blocks; pb >= mbl->num_blocks - MTK_BBT_MAX_BLOCK_NUM; pb--)
	{
		addr = pb << mbl->bott_mtd->erasesize_shift;

		if (mtd_block_isbad(mbl->bott_mtd, addr))
			continue;

		ret = mtd_read_oob(mbl->bott_mtd, addr, &ops);

		if (ret < 0 && ret != -EUCLEAN)
			continue;

		if (!memcmp(sig + MTK_BBT_SIG_OFF, mtk_bbt_sig, sizeof(mtk_bbt_sig) - 1))
		{
			if (!mbl_read_bbt_skip_bad(mbl, pb))
			{
				debug("Factory BBT found at block %d (0x%08llx)\n", pb, addr);
				mbl->bbt_block = pb;
				bbt_exist = 1;
				break;
			}
		}
	}

	if (!bbt_exist)
	{
		memset(mbl->bbt, 0, mbl->bbt_size);
		*bbtwb = 1;
	}

	/* Scan for bad blocks not marked in BBT */
	for (pb = 0; pb < mbl->num_blocks; pb++)
	{
		addr = pb << mbl->bott_mtd->erasesize_shift;

		if (mtd_block_isbad(mbl->bott_mtd, addr))
		{
			if (!mbl_bbt_is_bad(mbl, pb))
			{
				mbl_bbt_mark_bad(mbl, pb);
				*bbtwb = 1;
			}
		}
	}

	/* Create Block Mapping Table (memory only) */
	mbl->bmt = (s32 *) malloc(mbl->num_blocks * sizeof(*mbl->bmt));
	if (!mbl->bmt)
	{
		printf("mtk_bbt_layer: Unable to allocate %d bytes for BMT\n", mbl->num_blocks * sizeof(*mbl->bmt));
		free(mbl->bbt);
		return -ENOMEM;
	}

	mbl_update_bmt(mbl);

	return 0;
}

static int mbl_erase_block_check(struct mtd_info *mtd, u32 pb)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	u32 page_per_block = mtd->erasesize >> mtd->writesize_shift;
	struct mtd_oob_ops ops;
	u8 buf[SZ_4K + SZ_1K];
	loff_t addr;
	u32 i, j;
	int ret;

	if (mtd->writesize + mtd->oobsize > sizeof(buf))
		return 1;

	memset(&ops, 0, sizeof(ops));

	addr = pb << mtd->erasesize_shift;

	for (i = 0; i < page_per_block; i++)
	{
		ops.datbuf = buf;
		ops.len = mtd->writesize;
		ops.oobbuf = buf + mtd->writesize;
		ops.ooblen = mtd->oobsize;
		ops.ooboffs = 0;
		ops.mode = MTD_OPS_RAW;

		ret = mtd_read_oob(mbl->bott_mtd, addr, &ops);
		if ((ret && ret != -EUCLEAN) || (ops.retlen != ops.len) || (ops.oobretlen != ops.ooblen))
			return 1;

		for (j = 0; j < mtd->writesize + mtd->oobsize; j++)
			if (buf[j] != 0xff)
				return 1;

		addr += mtd->writesize;
	}

	return 0;
}

static int mbl_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	struct erase_info ei;
	loff_t start, end;
	u32 pb, lb;
	int ret, retries = 0;

	start = instr->addr & ~mtd->erasesize_mask;
	end = ALIGN(instr->addr + instr->len, mtd->erasesize);

	ei = *instr;
	ei.mtd = mbl->bott_mtd;
	ei.next = NULL;
	ei.callback = NULL;

	while (start < end)
	{
		lb = start >> mtd->erasesize_shift;
		pb = mbl->bmt[lb];

		if ((s32) pb == -1)
		{
			printf("%s: no usable block at 0x%08llx\n",
				__func__, (loff_t) lb << mtd->erasesize_shift);
			instr->state = MTD_ERASE_FAILED;
			instr->fail_addr = (loff_t) lb << mtd->erasesize_shift;
			goto erase_exit;
		}

		ei.addr = pb << mtd->erasesize_shift;
		ei.len = mtd->erasesize;
		debug("erasing lb %d (%08llx), pb %d (%08llx)\n", lb, start, pb, ei.addr);

	retry:
		ret = mtd_erase(mbl->bott_mtd, &ei);
		if (ret)
		{
			if (mbl_erase_block_check(mtd, pb))
			{
				if (retries < MTK_BBT_RETRIES)
				{
					nand_reset(mtd_to_nand(mbl->bott_mtd), 0);
					retries++;
					goto retry;
				}
				else
				{
					printf("New bad block during erasing: lb %d (%08llx), pb %d (%08llx)\n", lb, start, pb, ei.addr);

					mbl->bott_mtd->_block_markbad(mbl->bott_mtd, pb << mtd->erasesize_shift);
					mbl_bbt_mark_bad(mbl, pb);

					mbl_update_bmt(mbl);

					continue;
				}
			}
		}

		instr->state = ei.state;
		if (ei.state & MTD_ERASE_FAILED)
		{
			if (ei.fail_addr != MTD_FAIL_ADDR_UNKNOWN)
				instr->fail_addr = start;
			else
				instr->fail_addr = MTD_FAIL_ADDR_UNKNOWN;
		}

		start += mtd->erasesize;
	}

erase_exit:

	ret = instr->state == MTD_ERASE_DONE ? 0 : -EIO;

	/* Do call back function */
	if (!ret)
		mtd_erase_callback(instr);

	/* Return more or less happy */
	return ret;
}

static int mbl_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, uint8_t *buf)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	struct mtd_ecc_stats old_stats;
	loff_t start, end, off;
	size_t size_to_read, size_read, _retlen;
	int ret, euclean = 0, retries = 0;
	u32 pb, lb;

	start = from;
	end = from + len;
	size_read = 0;

	while (start < end)
	{
		lb = start >> mtd->erasesize_shift;
		pb = mbl->bmt[lb];
		if ((s32) pb == -1)
		{
			printf("%s: no usable block at 0x%08llx\n",
				__func__, (loff_t) lb << mtd->erasesize_shift);
			mtd->ecc_stats = mbl->bott_mtd->ecc_stats;
			*retlen = size_read;
			return -EIO;
		}

		off = start & mtd->erasesize_mask;
		size_to_read = mtd->erasesize - off;
		if (len - size_read < size_to_read)
			size_to_read = len - size_read;

		debug("reading lb %d (%08llx), pb %d (%08llx), size %08x\n", lb, start, pb, (pb << mtd->erasesize_shift) | off, size_to_read);

		old_stats = mbl->bott_mtd->ecc_stats;
	retry:
		ret = mtd_read(mbl->bott_mtd, (pb << mtd->erasesize_shift) | off, size_to_read, &_retlen, buf + size_read);
		if ((ret && ret != -EUCLEAN) || _retlen != size_to_read)
		{
			if (ret == -EBADMSG)
			{
				mtd->ecc_stats = mbl->bott_mtd->ecc_stats;
				*retlen = size_read + _retlen;
				debug("read with ecc failed, %d bytes read\n", _retlen);
				return -EBADMSG;
			}

			mbl->bott_mtd->ecc_stats = old_stats;
			if (retries < MTK_BBT_RETRIES)
			{
				nand_reset(mtd_to_nand(mbl->bott_mtd), 0);
				retries++;
				goto retry;
			}
			else
			{
				printf("New bad block during reading: lb %d (%08llx), pb %d (%08llx)\n", lb, start, pb, (pb << mtd->erasesize_shift) | off);

				mbl->bott_mtd->_block_markbad(mbl->bott_mtd, pb << mtd->erasesize_shift);
				mbl_bbt_mark_bad(mbl, pb);

				mbl_update_bmt(mbl);

				continue;
			}
		}

		if (ret == -EUCLEAN)
			euclean = 1;

		size_read += size_to_read;
		start += size_to_read;
	}

	mtd->ecc_stats = mbl->bott_mtd->ecc_stats;
	*retlen = len;

	return euclean ? -EUCLEAN : 0;
}

static int mbl_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const uint8_t *buf)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	loff_t start, end, off;
	size_t size_to_write, size_written, _retlen;
	u32 pb, lb;
	int ret, retries = 0;

	start = to;
	end = to + len;
	size_written = 0;

	while (start < end)
	{
		lb = start >> mtd->erasesize_shift;
		pb = mbl->bmt[lb];
		if ((s32) pb == -1)
		{
			printf("%s: no usable block at 0x%08llx\n",
				__func__, (loff_t) lb << mtd->erasesize_shift);
			*retlen = size_written;
			return -EIO;
		}

		off = start & mtd->erasesize_mask;
		size_to_write = mtd->erasesize - off;
		if (len - size_written < size_to_write)
			size_to_write = len - size_written;

		debug("writting lb %d (%08llx), pb %d (%08llx), size %08x\n", lb, start, pb, (pb << mtd->erasesize_shift) | off, size_to_write);

	retry:
		ret = mtd_write(mbl->bott_mtd, (pb << mtd->erasesize_shift) | off, size_to_write, &_retlen, buf + size_written);
		if (ret || _retlen != size_to_write)
		{
			if (retries < MTK_BBT_RETRIES)
			{
				nand_reset(mtd_to_nand(mbl->bott_mtd), 0);
				retries++;
				goto retry;
			}
			else
			{
				printf("New bad block during reading: lb %d (%08llx), pb %d (%08llx)\n", lb, start, pb, (pb << mtd->erasesize_shift) | off);

				mbl->bott_mtd->_block_markbad(mbl->bott_mtd, pb << mtd->erasesize_shift);
				mbl_bbt_mark_bad(mbl, pb);

				mbl_update_bmt(mbl);

				continue;
			}
		}

		size_written += size_to_write;
		start += size_to_write;
	}

	*retlen = len;

	return 0;
}

static int mbl_panic_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const uint8_t *buf)
{
	return mbl_write(mtd, to, len, retlen, buf);
}

static int mbl_read_oob(struct mtd_info *mtd, loff_t from,
	struct mtd_oob_ops *ops)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	u32 pb, lb;
	loff_t off;

	lb = from >> mtd->erasesize_shift;
	pb = mbl->bmt[lb];
	if ((s32) pb == -1)
	{
		printf("%s: no usable block at 0x%08llx\n",
			__func__, (loff_t) lb << mtd->erasesize_shift);
		return -EIO;
	}

	off = from & mtd->erasesize_mask;

	debug("reading oob lb %d (%08llx), pb %d (%08llx)\n", lb, from, pb, (pb << mtd->erasesize_shift) | off);

	return mtd_read_oob(mbl->bott_mtd, (pb << mtd->erasesize_shift) | off, ops);
}

static int mbl_write_oob(struct mtd_info *mtd, loff_t to,
	struct mtd_oob_ops *ops)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	u32 pb, lb;
	loff_t off;

	lb = to >> mtd->erasesize_shift;
	pb = mbl->bmt[lb];
	if ((s32) pb == -1)
	{
		printf("%s: no usable block at 0x%08llx\n",
			__func__, (loff_t) lb << mtd->erasesize_shift);
		return -EIO;
	}

	off = to & mtd->erasesize_mask;

	debug("writting oob lb %d (%08llx), pb %d (%08llx)\n", lb, to, pb, (pb << mtd->erasesize_shift) | off);

	return mtd_write_oob(mbl->bott_mtd, (pb << mtd->erasesize_shift) | off, ops);
}

static void mbl_sync(struct mtd_info *mtd)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);

	mtd_sync(mbl->bott_mtd);
}

static int mbl_block_isreserved(struct mtd_info *mtd, loff_t ofs)
{
	return 0;
}

static int mbl_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	u32 pb, lb;

	lb = offs >> mtd->erasesize_shift;
	pb = mbl->bmt[lb];
	if ((s32) pb == -1)
	{
		printf("%s: no usable block at 0x%08llx\n",
			__func__, (loff_t) lb << mtd->erasesize_shift);
		return 1;
	}

	return 0;
}

static int mbl_block_markbad(struct mtd_info *mtd, loff_t offs)
{
	return 0;
}

int create_mt7621_nand_bbt_layer(struct mtd_info *bott_mtd, struct mtd_info **upper_mtd)
{
	struct mtk_bbt_layer *mbl;
	int ret, bbtwb = 0;

	if (!bott_mtd)
		return -EINVAL;

	mbl = (struct mtk_bbt_layer *) malloc(sizeof(struct mtk_bbt_layer));
	if (!mbl)
		return -ENOMEM;

	memset(mbl, 0, sizeof(struct mtk_bbt_layer));

	mbl->bott_mtd = bott_mtd;

	/* Load BBT and scan */
	if ((ret = mbl_init_bbt(mbl, &bbtwb)) < 0)
		return ret;

	/* Create new MTD device */
	mbl->mtd.size = ((uint64_t) (mbl->avail_blocks - mbl->bbt_num_blocks) * bott_mtd->erasesize);

	mbl->mtd.type = bott_mtd->type;
	mbl->mtd.flags = bott_mtd->flags;
	mbl->mtd.erasesize = bott_mtd->erasesize;
	mbl->mtd.writesize = bott_mtd->writesize;
	mbl->mtd.writebufsize = bott_mtd->writebufsize;
	mbl->mtd.oobsize = bott_mtd->oobsize;
	mbl->mtd.oobavail = bott_mtd->oobavail;
	mbl->mtd.erasesize_shift = bott_mtd->erasesize_shift;
	mbl->mtd.writesize_shift = bott_mtd->writesize_shift;
	mbl->mtd.erasesize_mask = bott_mtd->erasesize_mask;
	mbl->mtd.writesize_mask = bott_mtd->writesize_mask;
	mbl->mtd.bitflip_threshold = bott_mtd->bitflip_threshold;
	mbl->mtd.ooblayout = bott_mtd->ooblayout;
	mbl->mtd.ecclayout = bott_mtd->ecclayout;
	mbl->mtd.ecc_step_size = bott_mtd->ecc_step_size;
	mbl->mtd.ecc_strength = bott_mtd->ecc_strength;
	mbl->mtd.numeraseregions = bott_mtd->numeraseregions;
	mbl->mtd.eraseregions = bott_mtd->eraseregions;
	mbl->mtd.subpage_sft = bott_mtd->subpage_sft;
	mbl->mtd.eraseregions = bott_mtd->eraseregions;

	if (bott_mtd->_erase)
		mbl->mtd._erase = mbl_erase;
	if (bott_mtd->_read)
		mbl->mtd._read = mbl_read;
	if (bott_mtd->_write)
		mbl->mtd._write = mbl_write;
	if (bott_mtd->_panic_write)
		mbl->mtd._panic_write = mbl_panic_write;
	if (bott_mtd->_read_oob)
		mbl->mtd._read_oob = mbl_read_oob;
	if (bott_mtd->_write_oob)
		mbl->mtd._write_oob = mbl_write_oob;
	if (bott_mtd->_sync)
		mbl->mtd._sync = mbl_sync;
	mbl->mtd._block_isreserved = mbl_block_isreserved;
	mbl->mtd._block_isbad = mbl_block_isbad;
	mbl->mtd._block_markbad = mbl_block_markbad;

	*upper_mtd = &mbl->mtd;

	if (bbtwb)
	{
		/* BBT Writeback */
		mbl_wb_bbt(&mbl->mtd);
	}

	return 0;
}

struct mtd_info *mbl_mtd_upper_to_lower(struct mtd_info *mtd)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	return mbl->bott_mtd;
}

u32 mbl_get_bbt_numblk(struct mtd_info *mtd)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);

	return mbl->num_blocks;
}

int mbl_get_bbt_blk_bad(struct mtd_info *mtd, u32 blockidx)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	u32 idx, pos;

	if (blockidx >= mbl->num_blocks)
		return -1;

	idx = blockidx / (8 / MTK_BBT_BITS_PER_BLOCK);
	pos = blockidx % (8 / MTK_BBT_BITS_PER_BLOCK);

	return ((mbl->bbt[idx] >> (pos * MTK_BBT_BITS_PER_BLOCK)) & ((1 << MTK_BBT_BITS_PER_BLOCK) - 1)) != MTK_BBT_GOOD_BLOCK_MASK;
}

u32 mbl_get_bmt(struct mtd_info *mtd, s32 **bmt)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);

	*bmt = mbl->bmt;
	return mbl->num_blocks;
}

void mbl_markbad_update(struct mtd_info *mtd, loff_t offs)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	u32 pb = offs >> mtd->erasesize_shift;

	mbl_bbt_mark_bad(mbl, pb);

	mbl_update_bmt(mbl);
}

void mbl_rescan_bbt_bmt(struct mtd_info *mtd)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	u32 pb;
	loff_t addr;

	memset(mbl->bbt, 0, mbl->bbt_size);

	/* Scan for bad blocks not marked in BBT */
	for (pb = 0; pb < mbl->num_blocks; pb++)
	{
		addr = pb << mbl->bott_mtd->erasesize_shift;

		if (mtd_block_isbad(mbl->bott_mtd, addr))
			mbl_bbt_mark_bad(mbl, pb);
	}

	mbl_update_bmt(mbl);
}

int mbl_wb_bbt(struct mtd_info *mtd)
{
	struct mtk_bbt_layer *mbl = mtd_to_mbl(mtd);
	int retries = 0;
	loff_t off;
	struct mtd_oob_ops ops;
	struct erase_info instr;

	memset(&instr, 0, sizeof(instr));

	instr.len = (loff_t) mbl->bbt_num_blocks << mbl->mtd.erasesize_shift;

	memset(&ops, 0, sizeof(ops));
	ops.datbuf = mbl->bbt;
	ops.oobbuf = (u8 *) mtk_bbt_sig;
	ops.len = mbl->bbt_size;
	ops.ooblen = sizeof(mtk_bbt_sig) - 1;
	ops.ooboffs = MTK_BBT_SIG_OFF;
	ops.mode = MTD_OPS_PLACE_OOB;

	while (retries++ < MTK_BBT_RETRIES)
	{
		off = (loff_t) (mbl->avail_blocks - mbl->bbt_num_blocks) << mbl->mtd.erasesize_shift;

		instr.addr = off;
		if (mbl_erase(&mbl->mtd, &instr))
			continue;

		if (instr.state != MTD_ERASE_DONE)
			continue;

		if (mbl_write_oob(&mbl->mtd, off, &ops))
			continue;

		if (ops.retlen != ops.len || ops.oobretlen != ops.ooblen)
			continue;

		return 0;
	}

	printf("%s: failed to write BBT\n", __func__);
	return 1;
}
