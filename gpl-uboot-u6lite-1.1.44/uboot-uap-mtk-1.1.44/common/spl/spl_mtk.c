
#include <common.h>
#include <spl.h>
#include <lzma/LzmaTypes.h>
#include <lzma/LzmaDec.h>
#include <lzma/LzmaTools.h>
#include <xyzModem.h>

#include <linux/sizes.h>

#if defined(CONFIG_MT7621_NAND_BOOT)
#include <nand.h>
#endif

#ifdef CONFIG_MIPS
#include <asm/addrspace.h>
#endif

#ifdef CONFIG_UBNT_APP
#include <ubnt_boot_param.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#define RESERVED_SPL_SIZE		SZ_1M

#ifndef CONFIG_SYS_BOOTM_LEN
/* use 8MByte as default max uncompress size */
#define CONFIG_SYS_BOOTM_LEN		SZ_8M
#endif

#define BUF_SIZE 1024

extern ulong get_mtk_image_search_start(void);
extern ulong get_mtk_image_search_end(void);
extern ulong get_mtk_image_search_sector_size(void);

extern char *get_mtk_stage2_image_ptr(char *data, size_t size);

static int is_valid_dram_mapped_address(u32 addr)
{
#if defined (CONFIG_MACH_MT7621)
	if (addr < 0x80001000 ||
		(addr >= 0x9c000000 && addr < 0xa0001000) ||
		addr > 0xbc000000)
		return 0;
#endif

	return 1;
}

static ulong free_dram_bottom(void)
{
#if defined (CONFIG_MACH_MT7621)
	return CONFIG_SYS_SDRAM_BASE + 0x1000;
#else
	return CONFIG_SYS_SDRAM_BASE;
#endif
}

static ulong free_dram_top(void)
{
	if (gd->ram_top)
		return gd->ram_top;

	return CONFIG_SYS_SDRAM_BASE + get_effective_memsize() - RESERVED_SPL_SIZE;
}

static int spl_mtk_try_load_image(struct spl_image_info *spl_image, ulong addr)
{
	int ret;
	struct image_header *uhdr, hdr;
	SizeT lzma_len;

	if (addr % sizeof (void *))
	{
		memcpy(&hdr, (const void *) addr, sizeof (hdr));
		uhdr = &hdr;
	}
	else
	{
		uhdr = (struct image_header *) addr;
	}

	ret = spl_parse_image_header(spl_image, uhdr);

	if (ret)
		return ret;

	if (!is_valid_dram_mapped_address(spl_image->load_addr))
	{
		debug("Error: Invalid load address found in image header: 0x%08lx\n", spl_image->load_addr);
		return -EINVAL;
	}

	if (!spl_image->entry_point)
		spl_image->entry_point = spl_image->load_addr;

	if (uhdr->ih_comp == IH_COMP_NONE)
	{
		/*
		* Load real U-Boot from its location to its
		* defined location in SDRAM
		*/

		if (spl_image->load_addr != (uintptr_t) (addr + sizeof(struct image_header)))
		{
			memmove((void *) (unsigned long) spl_image->load_addr,
				(void *) (addr + sizeof(struct image_header)),
				spl_image->size);
		}
	}
#ifdef CONFIG_SPL_LZMA
	else if (uhdr->ih_comp == IH_COMP_LZMA)
	{
		/*
		* Uncompress real U-Boot to its defined location in SDRAM
		*/
		lzma_len = CONFIG_SYS_BOOTM_LEN;

		ret = lzmaBuffToBuffDecompress((u8 *) spl_image->load_addr, &lzma_len,
			(u8 *) (addr + sizeof(struct image_header)), spl_image->size);

		if (ret)
		{
			printf("Error: LZMA uncompression error: %d\n", ret);
			return ret;
		}

		spl_image->size = lzma_len;
	}
#endif /* CONFIG_SPL_LZMA */
	else
	{
		debug("Warning: Unsupported compression method found in image header at offset 0x%08lx\n", addr);
		return -EINVAL;
	}
#ifdef CONFIG_UBNT_APP
	struct ubnt_boot_param *boot_param = \
		(struct ubnt_boot_param *)(((unsigned long) spl_image->load_addr) - sizeof(struct ubnt_boot_param));
	boot_param->image_search_start_addr = get_mtk_image_search_start();
	boot_param->image_search_end_addr = get_mtk_image_search_end();
	boot_param->image_search_sector_size = get_mtk_image_search_sector_size();
	flush_cache(((unsigned long) spl_image->load_addr) - sizeof(struct ubnt_boot_param),
		spl_image->size + sizeof(struct ubnt_boot_param));
#else
	flush_cache((unsigned long) spl_image->load_addr, spl_image->size);
#endif

	return 0;
}

static int spl_mtk_mmap_load_image(struct spl_image_info *spl_image,
	struct spl_boot_device *bootdev)
{
	ulong image_search_start = get_mtk_image_search_start();
	ulong image_search_end = get_mtk_image_search_end();
	ulong image_search_sector_size = get_mtk_image_search_sector_size();

	/*
	* Loading of the payload to SDRAM is done with skipping of
	* the mkimage header
	*/
	spl_image->flags |= SPL_COPY_PAYLOAD_ONLY;

	/* Try booting without padding */
	if (!spl_mtk_try_load_image(spl_image, image_search_start))
		return 0;

	image_search_start = ALIGN(image_search_start, image_search_sector_size);

	while (image_search_start < image_search_end)
	{
		if (!spl_mtk_try_load_image(spl_image, image_search_start))
			return 0;

		image_search_start += image_search_sector_size;
	}

	return -EINVAL;
}
SPL_LOAD_IMAGE_METHOD("MTK-MMAP", 0, BOOT_DEVICE_MTK_MMAP, spl_mtk_mmap_load_image);

static u32 spl_mtk_move_image(u32 src_addr, u32 load_addr, u32 data_size)
{
	u32 dst_addr;

#ifdef CONFIG_MIPS
	ulong k0_load_addr = CKSEG0ADDR(load_addr);
	ulong k0_mem_top = CKSEG0ADDR(free_dram_top());
	ulong k0_mem_bottom = CKSEG0ADDR(free_dram_bottom());

	if (k0_load_addr - k0_mem_bottom >= data_size)
		return src_addr;

	if (gd->ram_size < SZ_8M || (k0_load_addr + CONFIG_SYS_BOOTM_LEN > k0_mem_top))
	{
		dst_addr = free_dram_top() - data_size;
		dst_addr &= ~(SZ_4K - 1);
	}
	else
	{
		dst_addr = k0_load_addr + CONFIG_SYS_BOOTM_LEN;
	}

	memmove((void *) dst_addr, (void *) src_addr, data_size);
#else
	if (load_addr - free_dram_bottom() >= data_size)
		return src_addr;

	if (gd->ram_size < SZ_8M || (load_addr + CONFIG_SYS_BOOTM_LEN > free_dram_top()))
	{
		dst_addr = free_dram_top() - data_size;
		dst_addr &= ~(SZ_4K - 1);
	}
	else
	{
		dst_addr = load_addr + CONFIG_SYS_BOOTM_LEN;
	}

	memmove((void *) dst_addr, (void *) src_addr, data_size);
#endif

	return dst_addr;
}

#if defined(CONFIG_MT7621_NAND_BOOT)
static int spl_mtk_try_read_nand_image(struct spl_image_info *spl_image, ulong nand_addr, ulong *data_addr)
{
	struct image_header hdr;
	u32 old_crc;
	int ret;
	void *dst_addr;

	if (nand_spl_load_image(nand_addr, sizeof(hdr), &hdr))
		return -EINVAL;

	if (image_get_magic(&hdr) != IH_MAGIC)
		return -EINVAL;

	old_crc = image_get_hcrc(&hdr);
	hdr.ih_hcrc = 0;

	hdr.ih_hcrc = crc32(0, (u8 *) &hdr, sizeof(hdr));

	if (hdr.ih_hcrc != old_crc)
		return -EINVAL;

	ret = spl_parse_image_header(spl_image, &hdr);

	if (ret)
		return -EINVAL;

	if (!is_valid_dram_mapped_address(spl_image->load_addr))
	{
		printf("Error: invalid load address found in image header: 0x%08lx\n", spl_image->load_addr);
		return -EINVAL;
	}

	dst_addr = (void *) free_dram_bottom();

	if (nand_spl_load_image(nand_addr, sizeof(hdr) + image_get_data_size(&hdr), dst_addr))
		return -EINVAL;

	*data_addr = spl_mtk_move_image((ulong) dst_addr, spl_image->load_addr, sizeof(hdr) + image_get_data_size(&hdr));

	return 0;
}

static int spl_mtk_nand_load_image(struct spl_image_info *spl_image,
	struct spl_boot_device *bootdev)
{
	ulong image_search_start = get_mtk_image_search_start() + sizeof (struct image_header);
	ulong image_search_end = get_mtk_image_search_end();
	ulong image_search_sector_size = get_mtk_image_search_sector_size();
	ulong image_addr;

	nand_init();

	/*
	* Loading of the payload to SDRAM is done with skipping of
	* the mkimage header
	*/
	spl_image->flags |= SPL_COPY_PAYLOAD_ONLY;

	/* Try booting without padding */
	if (!spl_mtk_try_read_nand_image(spl_image, image_search_start, &image_addr))
	{
		if (!spl_mtk_try_load_image(spl_image, image_addr))
			return 0;
	}

	image_search_start = ALIGN(image_search_start, image_search_sector_size);

	while (image_search_start < image_search_end)
	{
		if (!spl_mtk_try_read_nand_image(spl_image, image_search_start, &image_addr))
		{
			if (!spl_mtk_try_load_image(spl_image, image_addr))
				return 0;
		}

		image_search_start += image_search_sector_size;
	}

	return -EINVAL;
}
SPL_LOAD_IMAGE_METHOD("MTK-NAND", 0, BOOT_DEVICE_MTK_NAND, spl_mtk_nand_load_image);
#endif /* CONFIG_MT7621_NAND_BOOT */

static int getcymodem(void)
{
	if (tstc())
		return (getc());
	return -1;
}

static int spl_mtk_ymodem_load_image(struct spl_image_info *spl_image, struct spl_boot_device *bootdev)
{
	int err, ret, size = 0;
	char *buf, *stage2_buf;
	ulong real_addr = 0;
	connection_info_t info;

	printf("\n");
	printf("Failed to load U-Boot image!\n");
	printf("Entering emergency mode.\n");
	printf("Please transmit a valid U-Boot image through this serial console.\n");
	printf("The U-Boot image will be booted up directly, and not be written to flash.\n");
	printf("Accepted mode is Ymoden-1K.\n");

	buf = (char *) free_dram_bottom();

	info.mode = xyzModem_ymodem;
	ret = xyzModem_stream_open(&info, &err);
	if (ret)
	{
		printf("spl: ymodem err - %s\n", xyzModem_error(err));
		return ret;
	}

	while ((ret = xyzModem_stream_read(buf + size, BUF_SIZE, &err)) > 0)
		size += ret;

	xyzModem_stream_close(&err);
	xyzModem_stream_terminate(false, &getcymodem);

	printf("Loaded %d bytes\n", size);

	stage2_buf = get_mtk_stage2_image_ptr(buf, size);

	if (!stage2_buf)
		return -EINVAL;

	ret = spl_parse_image_header(spl_image,
		(struct image_header *)stage2_buf);

	if (ret)
		return -EINVAL;

	if (!is_valid_dram_mapped_address(spl_image->load_addr))
	{
		printf("Error: invalid load address found in image header: 0x%08lx\n", spl_image->load_addr);
		return -EINVAL;
	}

	real_addr = spl_mtk_move_image((ulong) stage2_buf, spl_image->load_addr, size);

	return spl_mtk_try_load_image(spl_image, real_addr);
}
SPL_LOAD_IMAGE_METHOD("MTK-UART", 0, BOOT_DEVICE_MTK_UART, spl_mtk_ymodem_load_image);
