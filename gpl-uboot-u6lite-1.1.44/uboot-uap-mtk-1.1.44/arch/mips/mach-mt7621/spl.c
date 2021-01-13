/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spl.h>
#include <image.h>
#include <nand.h>
#include <u-boot/crc-p1003d2.h>
#include <mach/mt7621_regs.h>

#include <linux/sizes.h>
#include <linux/mtd/mtd.h>
#include <asm/byteorder.h>
#include <asm/addrspace.h>

DECLARE_GLOBAL_DATA_PTR;

#define MT7621_IH_NMLEN			12

#ifdef CONFIG_MT7621_NAND_BOOT
#define MT7621_BOOT_PARTITION_SIZE	0x80000
#else
#define MT7621_BOOT_PARTITION_SIZE	0x40000
#endif

typedef struct nand_header
{
	uint32_t	nand_ac_timing;
	uint32_t	ih_stage_offset;		/* Stage1 offset */
	uint32_t	ih_bootloader_offset;
	uint32_t	nand_info_1_data;
	uint32_t	crc;
} nand_header_t;

typedef struct mt7621_image_header
{
	uint32_t	ih_magic;			/* Image Header Magic Number	*/
	uint32_t	ih_hcrc;			/* Image Header CRC Checksum	*/
	uint32_t	ih_time;			/* Image Creation Timestamp	*/
	uint32_t	ih_size;			/* Image Data Size		*/
	uint32_t	ih_load;			/* Data	 Load  Address		*/
	uint32_t	ih_ep;				/* Entry Point Address		*/
	uint32_t	ih_dcrc;			/* Image Data CRC Checksum	*/
	uint8_t		ih_os;				/* Operating System		*/
	uint8_t		ih_arch;			/* CPU architecture		*/
	uint8_t		ih_type;			/* Image Type			*/
	uint8_t		ih_comp;			/* Compression Type		*/
	uint8_t		ih_name[MT7621_IH_NMLEN];	/* Image Name		*/
	nand_header_t	ih_nand;
} mt7621_image_header_t;

extern void cpu_secondary_init(void);

#ifdef CONFIG_SPL_BUILD
extern struct mtk_spl_rom_cfg __rom_cfg;
extern char __text_start;
extern char __image_copy_end;

static ulong init_spl_load_info(ulong reloc_size)
{
	u32 rom_size;

	gd->arch.image_search_end = CONFIG_SPL_TEXT_BASE + MT7621_BOOT_PARTITION_SIZE;

	if (__rom_cfg.magic != MTK_ROM_CFG_MAGIC_MT7621)
	{
		puts("Warning: invalid SPL rom magic!\n");
		rom_size = (ulong) (&__image_copy_end - &__text_start);
		gd->arch.image_search_align = CONFIG_MT7621_SPL_FLASH_SECTOR_SIZE;
	}
	else
	{
		rom_size = __rom_cfg.size;
		gd->arch.image_search_align = __rom_cfg.align;
	}

	gd->arch.image_search_start = CONFIG_SPL_TEXT_BASE + rom_size + reloc_size;

	if (gd->arch.image_search_start >= gd->arch.image_search_end)
	{
		puts("Warning: invalid SPL size parameter!\n");
		gd->arch.image_search_start = CONFIG_SPL_TEXT_BASE + CONFIG_MT7621_SPL_FLASH_SECTOR_SIZE;
	}

	return rom_size;
}

void __noreturn board_init_f(ulong dummy)
{
	const char *reloc_magic = (const char *) &__image_copy_end;
	uint32_t reloc_size = 0, rom_size;
	ulong relocaddr;
	ulong malloc_base;
	ulong start_addr_sp;
	gd_t *pnew_gd;

	preloader_console_init();

	if (!strncmp(reloc_magic, "RELS", 4))
	{
		memcpy(&reloc_size, reloc_magic + 4, 4);

		if (strncmp(reloc_magic + reloc_size - 4, "RELF", 4))
			reloc_size = 0;

		if (reloc_size && (__rom_cfg.reloc_size != reloc_size))
		{
			debug("Warning: reloc size in rom_cfg is not equal to its real size!\n");
			if (__rom_cfg.reloc_size > reloc_size)
				reloc_size = __rom_cfg.reloc_size;
		}
	}

	rom_size = init_spl_load_info(reloc_size);

	if (!reloc_size)
	{
		gd->malloc_base = CONFIG_SYS_SDRAM_BASE + get_effective_memsize() - CONFIG_VAL(SYS_MALLOC_F_LEN);
		board_init_r(NULL, 0);
	}

	relocaddr = CONFIG_SYS_SDRAM_BASE + get_effective_memsize() - rom_size;
	relocaddr &= ~(SZ_64K - 1);

	malloc_base = relocaddr - CONFIG_VAL(SYS_MALLOC_F_LEN) - SZ_4K;

	pnew_gd = (gd_t *) (malloc_base - sizeof (gd_t) - SZ_4K);
	pnew_gd = (gd_t *) ((ulong) pnew_gd & (~(SZ_4K - 1)));

	start_addr_sp = (ulong) pnew_gd - SZ_4K;

	memset(pnew_gd, 0, sizeof (gd_t));

	*pnew_gd = *gd;

	pnew_gd->malloc_base = malloc_base;

	asm volatile("move $26, %0" : : "r"(pnew_gd));

	relocate_code(start_addr_sp, pnew_gd, (ulong) relocaddr);
}

ulong get_mtk_image_search_start(void)
{
#ifdef CONFIG_MT7621_NAND_BOOT
	return gd->arch.image_search_start - CONFIG_SPL_TEXT_BASE;
#else
	return gd->arch.image_search_start;
#endif
}

ulong get_mtk_image_search_end(void)
{
#ifdef CONFIG_MT7621_NAND_BOOT
	return gd->arch.image_search_end - CONFIG_SPL_TEXT_BASE;
#else
	return gd->arch.image_search_end;
#endif
}

ulong get_mtk_image_search_sector_size(void)
{
	return gd->arch.image_search_align;
}
#endif

char *get_mtk_stage2_image_ptr(char *data, size_t size)
{
	struct image_header hdr;
#ifdef CONFIG_MT7621_NAND_BOOT
	mt7621_image_header_t *mt7621_uhdr;
#endif
	u32 old_chksum;
	struct mtk_spl_rom_cfg rom_cfg;
	u32 off;

	memcpy(&hdr, data, sizeof(hdr));

	if (image_get_magic(&hdr) == IH_MAGIC)
	{
		old_chksum = image_get_hcrc(&hdr);
		hdr.ih_hcrc = 0;

		if (crc32(0, (const u8 *) &hdr, sizeof(hdr)) != old_chksum)
			return NULL;

#ifdef CONFIG_MT7621_NAND_BOOT
		/* For NAND boot, determine whether this is the SPL part */
		mt7621_uhdr = (mt7621_image_header_t *) &hdr;

		old_chksum = uimage_to_cpu(mt7621_uhdr->ih_nand.crc);
		mt7621_uhdr->ih_nand.crc = 0;

		if (crc_p1003d2((uint8_t *) mt7621_uhdr, sizeof(*mt7621_uhdr)) != old_chksum)
#endif
			return data;
	}

	/* Locate stage2 */
#ifdef CONFIG_MT7621_NAND_BOOT
	memcpy(&rom_cfg, data + sizeof(*mt7621_uhdr) + MTK_SPL_ROM_CFG_OFFS, sizeof(rom_cfg));
#else
	memcpy(&rom_cfg, data + MTK_SPL_ROM_CFG_OFFS, sizeof(rom_cfg));
#endif

	if (rom_cfg.magic != MTK_ROM_CFG_MAGIC_MT7621)
		return NULL;

	off = rom_cfg.size + rom_cfg.reloc_size;

#ifdef CONFIG_MT7621_NAND_BOOT
	off += sizeof(*mt7621_uhdr);
#endif

	off = ALIGN(off, rom_cfg.align);

	while (off < size)
	{
		memcpy(&hdr, data + off, sizeof(hdr));

		if (image_get_magic(&hdr) == IH_MAGIC)
		{
			old_chksum = image_get_hcrc(&hdr);
			hdr.ih_hcrc = 0;

			if (crc32(0, (const u8 *) &hdr, sizeof(hdr)) == old_chksum)
				return data + off;
		}

		off += rom_cfg.align;
	}

	return NULL;
}

#ifndef CONFIG_MT7621_NAND_BOOT
int check_mtk_stock_stage2_info(size_t *stage2_off_min, size_t *stage2_off, size_t *align_size)
{
	struct image_header hdr;
	struct mtk_spl_rom_cfg rom_cfg;
	u32 off, addr, old_chksum;

	memcpy(&rom_cfg, (char *) CKSEG1ADDR(MT7621_FLASH_MMAP_BASE + MTK_SPL_ROM_CFG_OFFS), sizeof(rom_cfg));

	if (rom_cfg.magic != MTK_ROM_CFG_MAGIC_MT7621)
		return 0;

	off = rom_cfg.size + rom_cfg.reloc_size;
	off = ALIGN(off, rom_cfg.align);

	addr = off;

	while (addr < MT7621_BOOT_PARTITION_SIZE)
	{
		memcpy(&hdr, (char *) CKSEG1ADDR(MT7621_FLASH_MMAP_BASE + addr), sizeof(hdr));

		if (image_get_magic(&hdr) == IH_MAGIC)
		{
			old_chksum = image_get_hcrc(&hdr);
			hdr.ih_hcrc = 0;

			if (crc32(0, (const u8 *) &hdr, sizeof(hdr)) == old_chksum)
			{
				*stage2_off_min = off;
				*stage2_off = addr;
				*align_size = rom_cfg.align;
				return 1;
			}
		}

		addr += rom_cfg.align;
	}

	*stage2_off_min = off;
	*stage2_off = off;
	*align_size = rom_cfg.align;
	return 1;
}
#else
int check_mtk_stock_stage2_info(size_t *stage2_off_min, size_t *stage2_off, size_t *align_size)
{
	struct image_header hdr;
	mt7621_image_header_t *mt7621_uhdr;
	struct mtk_spl_rom_cfg rom_cfg;
	struct mtd_info *mtd;
	u32 off, addr, old_chksum;
	size_t retlen;
	int ret;

	mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return 0;

	ret = mtd_read(mtd, 0, sizeof(hdr), &retlen, (u8 *) &hdr);
	if ((ret && ret != -EUCLEAN) || (retlen != sizeof(hdr)))
		return 0;

	if (image_get_magic(&hdr) == IH_MAGIC)
	{
		old_chksum = image_get_hcrc(&hdr);
		hdr.ih_hcrc = 0;

		if (crc32(0, (const u8 *) &hdr, sizeof(hdr)) != old_chksum)
			return 0;

		/* For NAND boot, determine whether this is the SPL part */
		mt7621_uhdr = (mt7621_image_header_t *) &hdr;

		old_chksum = uimage_to_cpu(mt7621_uhdr->ih_nand.crc);
		mt7621_uhdr->ih_nand.crc = 0;

		if (crc_p1003d2((uint8_t *) mt7621_uhdr, sizeof(*mt7621_uhdr)) != old_chksum)
			return 0;
	}

	ret = mtd_read(mtd, sizeof (struct image_header) + MTK_SPL_ROM_CFG_OFFS, sizeof(rom_cfg), &retlen, (u8 *) &rom_cfg);
	if ((ret && ret != -EUCLEAN) || (retlen != sizeof(rom_cfg)))
		return 0;

	if (rom_cfg.magic != MTK_ROM_CFG_MAGIC_MT7621)
		return 0;

	off = rom_cfg.size + rom_cfg.reloc_size + sizeof (mt7621_image_header_t);
	off = ALIGN(off, rom_cfg.align);

	addr = off;

	while (addr < MT7621_BOOT_PARTITION_SIZE)
	{
		ret = mtd_read(mtd, addr, sizeof(hdr), &retlen, (u8 *) &hdr);
		if ((!ret || ret == -EUCLEAN) && (retlen == sizeof(hdr)))
		{
			if (image_get_magic(&hdr) == IH_MAGIC)
			{
				old_chksum = image_get_hcrc(&hdr);
				hdr.ih_hcrc = 0;

				if (crc32(0, (const u8 *) &hdr, sizeof(hdr)) == old_chksum)
				{
					*stage2_off_min = off;
					*stage2_off = addr;
					*align_size = rom_cfg.align;
					return 1;
				}
			}
		}

		addr += rom_cfg.align;
	}

	*stage2_off_min = off;
	*stage2_off = off;
	*align_size = rom_cfg.align;
	return 1;
}
#endif
