/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <nand.h>

#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/sizes.h>

#include "mt7621_nand.h"

#define ECC_ENCCON_REG16				0x000
#define   ENC_EN_S					  0
#define   ENC_EN_M					  0x01
#define   ENC_EN					  REG_MASK(ENC_EN)


#define ECC_ENCCNFG_REG32				0x004
#define   ENC_CNFG_MSG_S				  16
#define   ENC_CNFG_MSG_M				  0x1fff
#define   ENC_BURST_EN_S				  8
#define   ENC_BURST_EN_M				  0x01
#define   ENC_BURST_EN					  REG_MASK(ENC_BURST_EN)
#define   ENC_MODE_S					  4
#define   ENC_MODE_M					  0x03
#define     ENC_MODE_DMA				    0
#define     ENC_MODE_NFI				    1
#define     ENC_MODE_PIO				    2
#define   ENC_TNUM_S					  0
#define   ENC_TNUM_M					  0x07


#define ECC_ENCDIADDR_REG32				0x008


#define ECC_ENCIDLE_REG16				0x00c
#define   ENC_IDLE_S					  0
#define   ENC_IDLE_M					  0x01
#define   ENC_IDLE					  REG_MASK(ENC_IDLE)


#define ECC_ENCPAR0_REG32				0x010
#define ECC_ENCPAR1_REG32				0x014
#define ECC_ENCPAR2_REG32				0x018
#define ECC_ENCPAR3_REG32				0x01c
#define ECC_ENCPAR4_REG32				0x020


#define ECC_ENCSTA_REG32				0x024
#define   COUNT_MS_S					  16
#define   COUNT_MS_M					  0x3fff
#define   COUNT_PS_S					  7
#define   COUNT_PS_M					  0x1ff
#define   ENC_FSM_S					  0
#define   ENC_FSM_M					  0x3f


#define ECC_ENCIRQEN_REG16				0x028
#define ECC_ENCIRQSTA_REG16				0x02c
#define   ENC_IRQ_S					  0
#define   ENC_IRQ_M					  0x01
#define   ENC_IRQ					  REG_MASK(ENC_IRQ)


#define ECC_PIO_DIRDY_REG16				0x080
#define   PIO_DI_RDY_S					  0
#define   PIO_DI_RDY_M					  0x01
#define   PIO_DI_RDY					  REG_MASK(PIO_DI_RDY)


#define ECC_PIO_DI_REG32				0x084


#define ECC_DECCON_REG16				0x100
#define   DEC_EN_S					  0
#define   DEC_EN_M					  0x01
#define   DEC_EN					  REG_MASK(DEC_EN)


#define ECC_DECCNFG_REG32				0x104
#define   DEC_EMPTY_EN_S				  31
#define   DEC_EMPTY_EN_M				  0x01
#define   DEC_EMPTY_EN					  REG_MASK(DEC_EMPTY_EN)
#define   DEC_CS_S					  16
#define   DEC_CS_M					  0x1fff
#define   DEC_CON_S					  12
#define   DEC_CON_M					  0x03
#define     DEC_CON_FER					    1
#define     DEC_CON_EL					    2
#define     DEC_CON_CORRECT				    3
#define   DEC_BURST_EN_S				  8
#define   DEC_BURST_EN_M				  0x01
#define   DEC_BURST_EN					  REG_MASK(DEC_BURST_EN)
#define   DEC_MODE_S					  4
#define   DEC_MODE_M					  0x03
#define     DEC_MODE_DMA				    0
#define     DEC_MODE_NFI				    1
#define     DEC_MODE_PIO				    2
#define   DEC_TNUM_S					  0
#define   DEC_TNUM_M					  0x07


#define ECC_DECDIADDR_REG32				0x108


#define ECC_DECIDLE_REG16				0x10c
#define   DEC_IDLE_S					  0
#define   DEC_IDLE_M					  0x01
#define   DEC_IDLE					  REG_MASK(DEC_IDLE)


#define ECC_DECFER_REG16				0x110
#define   FER7_S					  7
#define   FER7_M					  0x01
#define   FER7						  REG_MASK(FER7)
#define   FER6_S					  6
#define   FER6_M					  0x01
#define   FER6						  REG_MASK(FER6)
#define   FER5_S					  5
#define   FER5_M					  0x01
#define   FER5						  REG_MASK(FER5)
#define   FER4_S					  4
#define   FER4_M					  0x01
#define   FER4						  REG_MASK(FER4)
#define   FER3_S					  3
#define   FER3_M					  0x01
#define   FER3						  REG_MASK(FER3)
#define   FER2_S					  2
#define   FER2_M					  0x01
#define   FER2						  REG_MASK(FER2)
#define   FER1_S					  1
#define   FER1_M					  0x01
#define   FER1						  REG_MASK(FER1)
#define   FER0_S					  0
#define   FER0_M					  0x01
#define   FER0						  REG_MASK(FER0)


#define ECC_DECENUM_REG32				0x114
#define   ERRNUM7_S					  28
#define   ERRNUM7_M					  0x0f
#define   ERRNUM6_S					  24
#define   ERRNUM6_M					  0x0f
#define   ERRNUM5_S					  20
#define   ERRNUM5_M					  0x0f
#define   ERRNUM4_S					  16
#define   ERRNUM4_M					  0x0f
#define   ERRNUM3_S					  12
#define   ERRNUM3_M					  0x0f
#define   ERRNUM2_S					  8
#define   ERRNUM2_M					  0x0f
#define   ERRNUM1_S					  4
#define   ERRNUM1_M					  0x0f
#define   ERRNUM0_S					  0
#define   ERRNUM0_M					  0x0f

#define   ERRNUM_S					  2
#define   ERRNUM_M					  0xf


#define ECC_DECDONE_REG16				0x118
#define   DONE7_S					  7
#define   DONE7_M					  0x01
#define   DONE7						  REG_MASK(DONE7)
#define   DONE6_S					  6
#define   DONE6_M					  0x01
#define   DONE6						  REG_MASK(DONE6)
#define   DONE5_S					  5
#define   DONE5_M					  0x01
#define   DONE5						  REG_MASK(DONE5)
#define   DONE4_S					  4
#define   DONE4_M					  0x01
#define   DONE4						  REG_MASK(DONE4)
#define   DONE3_S					  3
#define   DONE3_M					  0x01
#define   DONE3						  REG_MASK(DONE3)
#define   DONE2_S					  2
#define   DONE2_M					  0x01
#define   DONE2						  REG_MASK(DONE2)
#define   DONE1_S					  1
#define   DONE1_M					  0x01
#define   DONE1						  REG_MASK(DONE1)
#define   DONE0_S					  0
#define   DONE0_M					  0x01
#define   DONE0						  REG_MASK(DONE0)


#define ECC_DECEL0_REG32				0x11c
#define   DEC_EL1_S					  16
#define   DEC_EL1_M					  0x1fff
#define   DEC_EL0_S					  0
#define   DEC_EL0_M					  0x1fff


#define ECC_DECEL1_REG32				0x120
#define   DEC_EL3_S					  16
#define   DEC_EL3_M					  0x1fff
#define   DEC_EL2_S					  0
#define   DEC_EL2_M					  0x1fff


#define ECC_DECEL2_REG32				0x124
#define   DEC_EL5_S					  16
#define   DEC_EL5_M					  0x1fff
#define   DEC_EL4_S					  0
#define   DEC_EL4_M					  0x1fff


#define ECC_DECEL3_REG32				0x128
#define   DEC_EL7_S					  16
#define   DEC_EL7_M					  0x1fff
#define   DEC_EL6_S					  0
#define   DEC_EL6_M					  0x1fff


#define ECC_DECEL4_REG32				0x12c
#define   DEC_EL9_S					  16
#define   DEC_EL9_M					  0x1fff
#define   DEC_EL8_S					  0
#define   DEC_EL8_M					  0x1fff


#define ECC_DECEL5_REG32				0x130
#define   DEC_EL11_S					  16
#define   DEC_EL11_M					  0x1fff
#define   DEC_EL10_S					  0
#define   DEC_EL10_M					  0x1fff

#define ECC_DECEL_REG32(n)				(0x11c + (n) * 4)
#define   DEC_EL_EVEN_S					  0
#define   DEC_EL_ODD_S					  16
#define   DEC_EL_M					  0x1fff
#define   DEC_EL_BIT_POS_M				0x7
#define   DEC_EL_BYTE_POS_S				3


#define ECC_DECIRQEN_REG16				0x134
#define ECC_DECIRQSTA_REG16				0x138
#define   DEC_IRQ_S					  0
#define   DEC_IRQ_M					  0x01
#define   DEC_IRQ					  REG_MASK(DEC_IRQ)


#define ECC_FDMADDR_REG32				0x13c


#define ECC_DECFSM_REG32				0x140
#define   AUTOC_FSM_S					  24
#define   AUTOC_FSM_M					  0x1f
#define   CHIEN_FSM_S					  16
#define   CHIEN_FSM_M					  0x1f
#define   BMA_FSM_S					  8
#define   BMA_FSM_M					  0x1f
#define   SYN_FSM_S					  5
#define   SYN_FSM_M					  0x3f


#define ECC_SYNSTA_REG32				0x144
#define   SYN_SNUM_S					  29
#define   SYN_SNUM_M					  0x07
#define   DIBW_S					  20
#define   DIBW_M					  0x3f
#define   NFI_SEC_NUMNFI_SEC_NUM_S			  16
#define   NFI_SEC_NUMNFI_SEC_NUM_M			  0x07
#define   NFI_STR_SET_S					  15
#define   NFI_STR_SET_M					  0x01
#define   NFI_STR_SET					  REG_MASK(NFI_STR_SET)
#define   SYN_COUNT_CS_S				  0
#define   SYN_COUNT_CS_M				  0x3fff


#define ECC_NFIDIDECNFIDI_REG32				0x148


#define ECC_SYN0_REG32					0x14c
#define   DEC_SYN3_S					  16
#define   DEC_SYN3_M					  0x1fff
#define   DEC_SYN1_S					  0
#define   DEC_SYN1_M					  0x1fff


/* ENC_TNUM and DEC_TNUM */
#define     ECC_CAP_4B					    0
#define     ECC_CAP_6B					    1
#define     ECC_CAP_8B					    2
#define     ECC_CAP_10B					    3
#define     ECC_CAP_12B					    4

/* ENCIDLE and DECIDLE */
#define   ECC_IDLE_S					  0
#define   ECC_IDLE_M					  0x01
#define   ECC_IDLE					  REG_MASK(ECC_IDLE)

static int nfc_ecc_wait_idle(mt7621_nfc_t *nfc, u32 reg);

static void ecc_write16(mt7621_nfc_t *nfc, u32 addr, u16 val)
{
	return writew(val, nfc->ecc_base + addr);
}

static u32 ecc_read32(mt7621_nfc_t *nfc, u32 addr)
{
	return readl(nfc->ecc_base + addr);
}

static void ecc_write32(mt7621_nfc_t *nfc, u32 addr, u32 val)
{
	return writel(val, nfc->ecc_base + addr);
}

void nfc_ecc_init(mt7621_nfc_t *nfc, struct nand_ecc_ctrl *ecc)
{
	u32 ecc_cap_bit_cfg, encode_block_size, decode_block_size;

	switch (ecc->strength)
	{
	case 4:
		ecc_cap_bit_cfg = ECC_CAP_4B;
		break;
	case 6:
		ecc_cap_bit_cfg = ECC_CAP_6B;
		break;
	case 8:
		ecc_cap_bit_cfg = ECC_CAP_8B;
		break;
	case 10:
		ecc_cap_bit_cfg = ECC_CAP_10B;
		break;
	case 12:
		ecc_cap_bit_cfg = ECC_CAP_12B;
		break;
	default:
		ecc_cap_bit_cfg = ECC_CAP_4B;
		break;
	}

	/* Sector + FDM */
	encode_block_size = (ecc->size + NFI_ECC_FDM_SIZE) << 3;
	nfc_ecc_wait_idle(nfc, ECC_ENCIDLE_REG16);
	ecc_write16(nfc, ECC_ENCCON_REG16, 0);
	ecc_write32(nfc, ECC_ENCCNFG_REG32, REG_SET_VAL(ENC_TNUM, ecc_cap_bit_cfg) | REG_SET_VAL(ENC_MODE, ENC_MODE_NFI) | REG_SET_VAL(ENC_CNFG_MSG, encode_block_size));

	/* Sector + FDM + ECC parity bits */
	decode_block_size = ((ecc->size + NFI_ECC_FDM_SIZE) << 3) + ecc->strength * NFI_ECC_PARITY_BITS;
	nfc_ecc_wait_idle(nfc, ECC_DECIDLE_REG16);
	ecc_write16(nfc, ECC_DECCON_REG16, 0);
	ecc_write32(nfc, ECC_DECCNFG_REG32, REG_SET_VAL(DEC_TNUM, ecc_cap_bit_cfg) | REG_SET_VAL(DEC_MODE, DEC_MODE_NFI) | REG_SET_VAL(DEC_CS, decode_block_size) | DEC_EMPTY_EN | REG_SET_VAL(DEC_CON, DEC_CON_EL));

	/* setup FDM register base */
	ecc_write32(nfc, ECC_FDMADDR_REG32, CPHYSADDR((u32) nfc->nfi_base + NFI_FDM0L_REG32));
}

static int nfc_ecc_wait_idle(mt7621_nfc_t *nfc, u32 reg)
{
	u32 val;
	int ret;


	ret = readw_poll_timeout(nfc->ecc_base + reg, val,
		(val & ECC_IDLE), NFI_STATUS_WAIT_TIMEOUT_US);

	if (ret)
	{
		printf("Warning: ECC engine timed out entering idle mode\n");
		return -EIO;
	}

	return 0;
}

int nfc_ecc_decoder_wait_done(mt7621_nfc_t *nfc, u32 sector_index)
{
	u32 val;
	int ret;

	ret = readw_poll_timeout(nfc->ecc_base + ECC_DECDONE_REG16, val,
		(val & (1 << sector_index)), NFI_STATUS_WAIT_TIMEOUT_US);

	if (ret)
	{
		printf("Warning: ECC decoder for sector %d timed out\n", sector_index);
		return -ETIMEDOUT;
	}

	return 0;
}

void nfc_ecc_encoder_start(mt7621_nfc_t *nfc)
{
	nfc_ecc_wait_idle(nfc, ECC_ENCIDLE_REG16);
	ecc_write16(nfc, ECC_ENCCON_REG16, ENC_EN);
}

void nfc_ecc_encoder_stop(mt7621_nfc_t *nfc)
{
	nfc_ecc_wait_idle(nfc, ECC_ENCIDLE_REG16);
	ecc_write16(nfc, ECC_ENCCON_REG16, 0);
}

void nfc_ecc_decoder_start(mt7621_nfc_t *nfc)
{
	nfc_ecc_wait_idle(nfc, ECC_DECIDLE_REG16);
	ecc_write16(nfc, ECC_DECCON_REG16, DEC_EN);
}

void nfc_ecc_decoder_stop(mt7621_nfc_t *nfc)
{
	nfc_ecc_wait_idle(nfc, ECC_DECIDLE_REG16);
	ecc_write16(nfc, ECC_DECCON_REG16, 0);
}

int nfc_ecc_correct_check(mt7621_nfc_t *nfc, mt7621_nfc_sel_t *nfc_chip, u8 *sector_buf, u32 sector_index)
{
	struct nand_chip *nand = &nfc_chip->nand;
	struct mtd_info *mtd = nand_to_mtd(nand);
	int bitflips = 0;
	u32 i;
	u32 num_error_bits;
	u32 error_locations, error_bit_loc;
	u32 error_byte_pos, error_bit_pos_in_byte;

	num_error_bits = (ecc_read32(nfc, ECC_DECENUM_REG32) >> (sector_index << ERRNUM_S)) & ERRNUM_M;

	if (!num_error_bits)
		return 0;

	if (num_error_bits == ERRNUM_M)
	{
		mtd->ecc_stats.failed++;
		return -1;
	}

	for (i = 0; i < num_error_bits; i++)
	{
		error_locations = ecc_read32(nfc, ECC_DECEL_REG32(i / 2));
		error_bit_loc = (error_locations >> ((i % 2) * DEC_EL_ODD_S)) & DEC_EL_M;

		if (error_bit_loc < (nand->ecc.size << 3))
		{
			error_byte_pos = error_bit_loc >> DEC_EL_BYTE_POS_S;
			error_bit_pos_in_byte = error_bit_loc & DEC_EL_BIT_POS_M;
			sector_buf[error_byte_pos] ^= (1 << error_bit_pos_in_byte);

			bitflips++;
		}
	}

	mtd->ecc_stats.corrected += bitflips;

	return bitflips;
}
