/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2018
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*    Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
******************************************************************************/

#include "mkimage.h"

#include <sys/types.h>

#include <image.h>
#include <u-boot/crc.h>
#include <u-boot/crc-p1003d2.h>

#define MTK_SPL_ROM_CFG_OFFS		0x40

#define MTK_MAGIC_MT7621		0x31323637

#define MT7621_IH_NMLEN			12

struct mtk_spl_rom_cfg
{
	uint32_t	magic;
	uint32_t	size;
	uint32_t	align;
	uint32_t	reloc_size;
};

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

typedef struct stage1_header
{
	uint32_t	rsvd0;
	uint32_t	rsvd1;
	uint32_t	sh_ep;				/* Stage1 Entry Point */
	uint32_t	sh_size;			/* Stage1 Size */
	uint32_t	sh_has_stage2;			/* Has Stage2 */
	uint32_t	sh_next_ep;			/* Stage2 Entry Point */
	uint32_t	sh_next_size;			/* Stage2 Size */
	uint32_t	sh_next_offset;			/* Stage2 Offset */
} stage1_header_t;

enum pad_type
{
	PAD_MMAP_NOR,
	PAD_MT7621_NAND
};

static uint32_t spl_ep;

static const char *in_file;
static const char *out_file;
enum pad_type pad = PAD_MMAP_NOR;

static int spl_nor_pad(char *in_buff, long in_len, char **pout_buff, long *pout_len);
static int spl_mt7621_nand_pad(char *in_buff, long in_len, char **pout_buff, long *pout_len);

static void usage(void)
{
	fprintf(stderr,
		"      Options:\n"
		"          -i <fin>  Specify input SPL bin file\n"
		"          -o <fout> Specify output pad bin file\n"
		"          -t <type> Specify padding type\n"
		"                        Optional, default for Generic mmap-ed NOR boot padding"
		"          -e <ep>   Specify SPL entry point (hex)\n"
		"\n"
		"      Available padding types:\n"
		"          mt7621_nand    MT7621 NAND boot padding\n"
		"                             Requires -e to be set\n"
		);

	exit(0);
}

int main(int argc, char *argv[])
{
	int opt;
	int err;
	FILE *f;
	long fin_len, fout_len = 0;
	char *in_buff, *out_buff = NULL, *ptr;

	static const char *type = NULL;
	static bool spl_ep_set = 0;

	while ((opt = getopt(argc, argv, "i:o:t:e:")) != -1)
	{
		switch (opt)
		{
		case 'i':
			in_file = optarg;
			break;
		case 'o':
			out_file = optarg;
			break;
		case 'e':
			spl_ep = strtoul(optarg, &ptr, 16);
			if (*ptr)
			{
				fprintf(stderr, "Invalid entry point address %s\n", optarg);
				return EINVAL;
			}
			spl_ep_set = 1;
			break;
		case 't':
			type = optarg;
			break;
		default:
			usage();
		}
	}

	if (type)
	{
		if (!strcmp(type, "mt7621_nand"))
		{
			if (!spl_ep_set)
			{
				fprintf(stderr, "Error: Use MT7621 NAND padding but -e not set\n");
				return EINVAL;
			}

			pad = PAD_MT7621_NAND;
		}
		else
		{
			fprintf(stderr, "Error: Invalid padding type: %s\n", type);
			return EINVAL;
		}
	}

	f = fopen(in_file, "rb");
	if (!f)
	{
		fprintf(stderr, "Unable to open input file %s\n", in_file);
		return errno;
	}

	if ((err = fseek(f, 0, SEEK_END)))
	{
		fprintf(stderr, "Unable to seek input file\n");
		fclose(f);
		return err;
	}

	if ((fin_len = ftell(f)) < 0)
	{
		fprintf(stderr, "Unable to get input file size\n");
		fclose(f);
		return 1;
	}

	in_buff = malloc(fin_len);
	if (!in_buff)
	{
		fprintf(stderr, "Unable to alloc memory for input file\n");
		fclose(f);
		return 1;
	}

	if ((err = fseek(f, 0, SEEK_SET)))
	{
		fprintf(stderr, "Unable to seek input file\n");
		fclose(f);
		return err;
	}

	if (fread(in_buff, 1, fin_len, f) != fin_len)
	{
		fprintf(stderr, "Unable to read input file\n");
		fclose(f);
		return 1;
	}

	fclose(f);

	switch (pad)
	{
	case PAD_MMAP_NOR:
		err = spl_nor_pad(in_buff, fin_len, &out_buff, &fout_len);
		break;
	case PAD_MT7621_NAND:
		err = spl_mt7621_nand_pad(in_buff, fin_len, &out_buff, &fout_len);
		break;
	default:
		return EINVAL;
	}

	if (err)
		return err;

	f = fopen(out_file, "wb");
	if (!f) {
		fprintf(stderr, "Unable to open output file %s\n", out_file);
		return 1;
	}

	if (fwrite(out_buff, 1, fout_len, f) != fout_len) {
		fprintf(stderr, "Unable to write output file\n");
		fclose(f);
		return 1;
	}

	fclose(f);

	return 0;
}

static int spl_nor_pad(char *in_buff, long in_len, char **pout_buff, long *pout_len)
{
	long buf_len;
	char *buff;
	struct mtk_spl_rom_cfg rom_cfg;

	/* Fetch ROM config */
	memcpy(&rom_cfg, in_buff + MTK_SPL_ROM_CFG_OFFS, sizeof(rom_cfg));

	/* Check rom_cfg magic */
	switch (le32toh(rom_cfg.magic))
	{
	case MTK_MAGIC_MT7621:
		break;
	default:
		fprintf(stderr, "Not a MTK SPL file\n");
		return -EINVAL;
	}

	/* Calculate pad size */
	buf_len = in_len;
	buf_len += le32toh(rom_cfg.align);
	buf_len &= ~(le32toh(rom_cfg.align) - 1);

	buff = malloc(buf_len);
	if (!buff) {
		fprintf(stderr, "Unable to alloc memory for output file\n");
		return 1;
	}

	memset(buff, 0xff, buf_len);

	memcpy(buff, in_buff, in_len);

	/* Write reloc size */
	rom_cfg.reloc_size = in_len - rom_cfg.size;
	rom_cfg.reloc_size = htole32(rom_cfg.reloc_size);

	memcpy((uint32_t *) (buff + MTK_SPL_ROM_CFG_OFFS), &rom_cfg, sizeof(rom_cfg));

	*pout_buff = buff;
	*pout_len = buf_len;

	return 0;
}

static int spl_mt7621_nand_pad(char *in_buff, long in_len, char **pout_buff, long *pout_len)
{
	long buf_len;
	char *buff;
	struct mtk_spl_rom_cfg rom_cfg;
	mt7621_image_header_t uhdr;
	stage1_header_t shdr;

	/* Fetch ROM config */
	memcpy(&rom_cfg, in_buff + MTK_SPL_ROM_CFG_OFFS, sizeof(rom_cfg));

	/* Check rom_cfg magic */
	switch (le32toh(rom_cfg.magic))
	{
	case MTK_MAGIC_MT7621:
		break;
	default:
		fprintf(stderr, "Not a MT7621 SPL file\n");
		return -EINVAL;
	}

	/* Calculate pad size */
	buf_len = in_len + sizeof (mt7621_image_header_t);
	buf_len += le32toh(rom_cfg.align);
	buf_len &= ~(le32toh(rom_cfg.align) - 1);

	buff = malloc(buf_len);
	if (!buff) {
		fprintf(stderr, "Unable to alloc memory for output file\n");
		return 1;
	}

	memset(buff, 0xff, buf_len);

	memcpy(buff + sizeof(mt7621_image_header_t), in_buff, in_len);

	/* Write reloc size */
	rom_cfg.reloc_size = in_len - rom_cfg.size;
	rom_cfg.reloc_size = htole32(rom_cfg.reloc_size);

	memcpy((uint32_t *) (buff + sizeof(mt7621_image_header_t) + MTK_SPL_ROM_CFG_OFFS), &rom_cfg, sizeof(rom_cfg));

	/* Write stage1 header */
	memcpy(&shdr, buff + sizeof(mt7621_image_header_t), sizeof(stage1_header_t));

	shdr.sh_ep = htobe32(spl_ep);
	shdr.sh_size = htobe32(in_len);

	memcpy(buff + sizeof(mt7621_image_header_t), &shdr, sizeof(stage1_header_t));

	/* Write U-Boot image header */
	memset(&uhdr, 0, sizeof(uhdr));

	uhdr.ih_magic = htobe32(IH_MAGIC);
	uhdr.ih_time = htobe32(time(NULL));
	uhdr.ih_size = htobe32(in_len);
	uhdr.ih_load = htobe32(spl_ep);
	uhdr.ih_ep = htobe32(spl_ep);
	uhdr.ih_os = IH_OS_LINUX;
	uhdr.ih_arch = IH_ARCH_MIPS;
	uhdr.ih_type = IH_TYPE_STANDALONE;
	uhdr.ih_comp = IH_COMP_NONE;
	uhdr.ih_dcrc = htobe32(crc32(0, (uint8_t *) buff + sizeof(mt7621_image_header_t), in_len));

	strncpy((char *) uhdr.ih_name, "MT7621 NAND", sizeof(uhdr.ih_name));

	uhdr.ih_nand.ih_stage_offset = htobe32(sizeof(mt7621_image_header_t));

	uhdr.ih_nand.crc = htobe32(crc_p1003d2((uint8_t *) &uhdr, sizeof(mt7621_image_header_t)));

	uhdr.ih_hcrc = htobe32(crc32(0, (uint8_t *) &uhdr, sizeof(mt7621_image_header_t)));

	memcpy(buff, &uhdr, sizeof(mt7621_image_header_t));

	*pout_buff = buff;
	*pout_len = buf_len;

	return 0;
}
