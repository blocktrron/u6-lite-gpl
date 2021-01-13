/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _MT7621_NAND_H_
#define _MT7621_NAND_H_

#include <nand.h>

#define REG_SET_VAL(_name, _val) \
	(((_name ## _M) & (_val)) << (_name ## _S))

#define REG_GET_VAL(_name, _val) \
	(((_val) >> (_name ## _S)) & (_name ## _M))

#define REG_MASK(_name) \
	((_name ## _M) << (_name ## _S))

#define NFI_CNFG_REG16					0x000
#define   OP_MODE_S					  12
#define   OP_MODE_M					  0x07
#define     OP_IDLE					    0
#define     OP_READ					    1
#define     OP_SINGLE_READ				    2
#define     OP_PROGRAM					    3
#define     OP_ERASE					    4
#define     OP_RESET					    5
#define     OP_CUSTOM					    6
#define   AUTO_FMT_EN_S				  	  9
#define   AUTO_FMT_EN_M				  	  0x01
#define   AUTO_FMT_EN				  	  REG_MASK(AUTO_FMT_EN)
#define   HW_ECC_EN_S					  8
#define   HW_ECC_EN_M					  0x01
#define   HW_ECC_EN					  REG_MASK(HW_ECC_EN)
#define   BYTE_RW_S					  6
#define   BYTE_RW_M					  0x01
#define   BYTE_RW					  REG_MASK(BYTE_RW)
#define   DMA_BURST_EN_S				  2
#define   DMA_BURST_EN_M				  0x01
#define   DMA_BURST_EN					  REG_MASK(DMA_BURST_EN)
#define   READ_MODE_S					  1
#define   READ_MODE_M					  0x01
#define   READ_MODE					  REG_MASK(READ_MODE)
#define   DMA_MODE_S					  0
#define   DMA_MODE_M					  0x01
#define   DMA_MODE					  REG_MASK(DMA_MODE)


#define NFI_PAGEFMT_REG16				0x004
#define   PAGEFMT_FDM_ECC_S				  12
#define   PAGEFMT_FDM_ECC_M				  0xf
#define   PAGEFMT_FDM_S					  8
#define   PAGEFMT_FDM_M					  0xf
#define   PAGEFMT_SPARE_S				  4
#define   PAGEFMT_SPARE_M				  0x3
#define     PAGEFMT_SPARE_16				    0
#define     PAGEFMT_SPARE_26				    1
#define     PAGEFMT_SPARE_27				    2
#define     PAGEFMT_SPARE_28				    3
#define   PAGEFMT_DBYTE_S				  3
#define   PAGEFMT_DBYTE_M				  0x01
#define   PAGEFMT_DBYTE					  REG_MASK(PAGEFMT_DBYTE)
#define   PAGEFMT_PAGE_S				  0
#define   PAGEFMT_PAGE_M				  0x3
#define     PAGEFMT_512					    0
#define     PAGEFMT_2K					    1
#define     PAGEFMT_4K					    2


#define NFI_CON_REG16					0x008
#define   NFI_SEC_S					  12
#define   NFI_SEC_M					  0xf
#define   NFI_BWR_S					  9
#define   NFI_BWR_M					  0x01
#define   NFI_BWR					  REG_MASK(NFI_BWR)
#define   NFI_BRD_S					  8
#define   NFI_BRD_M					  0x01
#define   NFI_BRD					  REG_MASK(NFI_BRD)
#define   NFI_NOB_S					  5
#define   NFI_NOB_M					  0x07
#define   NFI_SRD_S					  4
#define   NFI_SRD_M					  0x01
#define   NFI_SRD					  REG_MASK(NFI_SRD)
#define   NFI_STM_RST_S					  1
#define   NFI_STM_RST_M					  0x01
#define   NFI_STM_RST					  REG_MASK(NFI_STM_RST)
#define   NFI_FIFO_FLUSH_S				  0
#define   NFI_FIFO_FLUSH_M				  0x01
#define   NFI_FIFO_FLUSH				  REG_MASK(NFI_FIFO_FLUSH)


#define NFI_ACCCON_REG32				0x00c
#define   POECS_S					  28
#define   POECS_M					  0x0f
#define   PRECS_S					  22
#define   PRECS_M					  0x3f
#define   C2R_S						  16
#define   C2R_M						  0x3f
#define   W2R_S						  12
#define   W2R_M						  0x0f
#define   WH_S						  8
#define   WH_M						  0x0f
#define   WST_S						  4
#define   WST_M						  0x0f
#define   RLT_S						  0
#define   RLT_M						  0x0f


#define NFI_INTR_EN_REG16				0x010
#define NFI_INTR_REG16					0x014
#define   AHB_DONE_S					  6
#define   AHB_DONE_M					  0x01
#define   AHB_DONE					  REG_MASK(AHB_DONE)
#define   ACCESS_LOCK_INTR_S				  5
#define   ACCESS_LOCK_INTR_M				  0x01
#define   ACCESS_LOCK					  REG_MASK(ACCESS_LOCK)
#define   BUSY_RETURN_S					  4
#define   BUSY_RETURN_M					  0x01
#define   BUSY_RETURN					  REG_MASK(BUSY_RETURN)
#define   ERASE_DONE_S					  3
#define   ERASE_DONE_M					  0x01
#define   ERASE_DONE					  REG_MASK(ERASE_DONE)
#define   RESET_DONE_S					  2
#define   RESET_DONE_M					  0x01
#define   RESET_DONE					  REG_MASK(RESET_DONE)
#define   WR_DONE_S					  1
#define   WR_DONE_M					  0x01
#define   WR_DONE					  REG_MASK(WR_DONE)
#define   RD_DONE_S					  0
#define   RD_DONE_M					  0x01
#define   RD_DONE					  REG_MASK(RD_DONE)


#define NFI_CMD_REG16					0x020
#define   CMD_S						  0
#define   CMD_M						  0xff


#define NFI_ADDRNOB_REG16				0x030
#define   ADDR_ROW_NOB_S				  4
#define   ADDR_ROW_NOB_M				  0x07
#define   ADDR_COL_NOB_S				  0
#define   ADDR_COL_NOB_M				  0x07


#define NFI_COLADDR_REG32				0x034
#define   COL_ADDR3_S					  24
#define   COL_ADDR3_M					  0xff
#define   COL_ADDR2_S					  16
#define   COL_ADDR2_M					  0xff
#define   COL_ADDR1_S					  8
#define   COL_ADDR1_M					  0xff
#define   COL_ADDR0_S					  0
#define   COL_ADDR0_M					  0xff


#define NFI_ROWADDR_REG32				0x038
#define   ROW_ADDR3_S					  24
#define   ROW_ADDR3_M					  0xff
#define   ROW_ADDR2_S					  16
#define   ROW_ADDR2_M					  0xff
#define   ROW_ADDR1_S					  8
#define   ROW_ADDR1_M					  0xff
#define   ROW_ADDR0_S					  0
#define   ROW_ADDR0_M					  0xff


#define NFI_STRDATA_REG16				0x040
#define   STR_DATA_S					  0
#define   STR_DATA_M					  0x01
#define   STR_DATA					  REG_MASK(STR_DATA)


#define NFI_CNRN_REG16					0x044
#define   CB2R_TIME_S					  4
#define   CB2R_TIME_M					  0xf
#define   STR_CNRNB_S					  0
#define   STR_CNRNB_M					  0x01
#define   STR_CNRNB					  REG_MASK(STR_CNRNB)


#define NFI_DATAW_REG32					0x050
#define   DW3_S						  24
#define   DW3_M						  0xff
#define   DW2_S						  16
#define   DW2_M						  0xff
#define   DW1_S						  8
#define   DW1_M						  0xff
#define   DW0_S						  0
#define   DW0_M						  0xff


#define NFI_DATAR_REG32					0x054
#define   DR3_S						  24
#define   DR3_M						  0xff
#define   DR2_S						  16
#define   DR2_M						  0xff
#define   DR1_S						  8
#define   DR1_M						  0xff
#define   DR0_S						  0
#define   DR0_M						  0xff


#define NFI_PIO_DIRDY_REG16				0x058
#define   PIO_DIRDY_S					  0
#define   PIO_DIRDY_M					  0x01
#define   PIO_DIRDY					  REG_MASK(PIO_DIRDY)


#define NFI_STA_REG32					0x060
#define   NAND_FSM_S					  24
#define   NAND_FSM_M					  0x1f
#define   NFI_FSM_S					  16
#define   NFI_FSM_M					  0x0f
#define     FSM_IDLE					    0
#define     FSM_RESET					    1
#define     FSM_RESET_BUSY				    2
#define     FSM_RESET_DATA				    3
#define     FSM_PROGRAM_BUSY				    4
#define     FSM_PROGRAM_DATA				    5
#define     FSM_ERASE_BUSY				    8
#define     FSM_ERASE_DATA				    9
#define     FSM_CUSTOM_DATA				    14
#define     FSM_CUSTOM_MODE				    15
#define   READ_EMPTY_S					  12
#define   READ_EMPTY_M					  0x01
#define   READ_EMPTY					  REG_MASK(READ_EMPTY)
#define   BUSY2READY_S					  9
#define   BUSY2READY_M					  0x01
#define   BUSY2READY					  REG_MASK(BUSY2READY)
#define   BUSY_S					  8
#define   BUSY_M					  0x01
#define   BUSY						  REG_MASK(BUSY)
#define   ACCESS_LOCK_S					  4
#define   ACCESS_LOCK_M					  0x01
#define   ACCESS_LOCK					  REG_MASK(ACCESS_LOCK)
#define   DATAW_S					  3
#define   DATAW_M					  0x01
#define   DATAW						  REG_MASK(DATAW)
#define   DATAR_S					  2
#define   DATAR_M					  0x01
#define   DATAR						  REG_MASK(DATAR)
#define   ADDR_S					  1
#define   ADDR_M					  0x01
#define   ADDR						  REG_MASK(ADDR)
#define   STA_CMD_S					  0
#define   STA_CMD_M					  0x01
#define   STA_CMD					  REG_MASK(STA_CMD)


#define NFI_FIFOSTA_REG16				0x064
#define   WR_FULL_S					  15
#define   WR_FULL_M					  0x01
#define   WR_FULL					  REG_MASK(WR_FULL)
#define   WR_EMPTY_S					  14
#define   WR_EMPTY_M					  0x01
#define   WR_EMPTY					  REG_MASK(WR_EMPTY)
#define   WR_REMAIN_S					  8
#define   WR_REMAIN_M					  0x1f
#define   RD_FULL_S					  7
#define   RD_FULL_M					  0x01
#define   RD_FULL					  REG_MASK(RD_FULL)
#define   RD_EMPTY_S					  6
#define   RD_EMPTY_M					  0x01
#define   RD_EMPTY					  REG_MASK(RD_EMPTY)
#define   RD_REMAIN_S					  0
#define   RD_REMAIN_M					  0x1f


#define NFI_LOCKSTA_REG16				0x68
#define   ACCESS_LOCK15_S				  15
#define   ACCESS_LOCK15_M				  0x01
#define   ACCESS_LOCK15					  REG_MASK(ACCESS_LOCK15)
#define   ACCESS_LOCK14_S				  14
#define   ACCESS_LOCK14_M				  0x01
#define   ACCESS_LOCK14					  REG_MASK(ACCESS_LOCK14)
#define   ACCESS_LOCK13_S				  13
#define   ACCESS_LOCK13_M				  0x01
#define   ACCESS_LOCK13					  REG_MASK(ACCESS_LOCK13)
#define   ACCESS_LOCK12_S				  12
#define   ACCESS_LOCK12_M				  0x01
#define   ACCESS_LOCK12					  REG_MASK(ACCESS_LOCK12)
#define   ACCESS_LOCK11_S				  11
#define   ACCESS_LOCK11_M				  0x01
#define   ACCESS_LOCK11					  REG_MASK(ACCESS_LOCK11)
#define   ACCESS_LOCK10_S				  10
#define   ACCESS_LOCK10_M				  0x01
#define   ACCESS_LOCK10					  REG_MASK(ACCESS_LOCK10)
#define   ACCESS_LOCK9_S				  9
#define   ACCESS_LOCK9_M				  0x01
#define   ACCESS_LOCK9					  REG_MASK(ACCESS_LOCK9)
#define   ACCESS_LOCK8_S				  8
#define   ACCESS_LOCK8_M				  0x01
#define   ACCESS_LOCK8					  REG_MASK(ACCESS_LOCK8)
#define   ACCESS_LOCK7_S				  7
#define   ACCESS_LOCK7_M				  0x01
#define   ACCESS_LOCK7					  REG_MASK(ACCESS_LOCK7)
#define   ACCESS_LOCK6_S				  6
#define   ACCESS_LOCK6_M				  0x01
#define   ACCESS_LOCK6					  REG_MASK(ACCESS_LOCK6)
#define   ACCESS_LOCK5_S				  5
#define   ACCESS_LOCK5_M				  0x01
#define   ACCESS_LOCK5					  REG_MASK(ACCESS_LOCK5)
#define   ACCESS_LOCK4_S				  4
#define   ACCESS_LOCK4_M				  0x01
#define   ACCESS_LOCK4					  REG_MASK(ACCESS_LOCK4)
#define   ACCESS_LOCK3_S				  3
#define   ACCESS_LOCK3_M				  0x01
#define   ACCESS_LOCK3					  REG_MASK(ACCESS_LOCK3)
#define   ACCESS_LOCK2_S				  2
#define   ACCESS_LOCK2_M				  0x01
#define   ACCESS_LOCK2					  REG_MASK(ACCESS_LOCK2)
#define   ACCESS_LOCK1_S				  1
#define   ACCESS_LOCK1_M				  0x01
#define   ACCESS_LOCK1					  REG_MASK(ACCESS_LOCK1)
#define   ACCESS_LOCK0_S				  0
#define   ACCESS_LOCK0_M				  0x01
#define   ACCESS_LOCK0					  REG_MASK(ACCESS_LOCK0)


#define NFI_ADDRCNTR_REG16				0x070
#define   SEC_CNTR_S					  12
#define   SEC_CNTR_M					  0xf
#define   SEC_ADDR_S					  0
#define   SEC_ADDR_M					  0x3ff


#define NFI_STRADDR_REG32				0x080


#define NFI_BYTELEN_REG16				0x084
#define   BUS_SEC_CNTR_S				  12
#define   BUS_SEC_CNTR_M				  0xf
#define   BUS_SEC_ADDR_S				  0
#define   BUS_SEC_ADDR_M				  0x3ff


#define NFI_CSEL_REG16					0x090
#define   CSEL_S					  0
#define   CSEL_M					  0x01
#define   CSEL						  REG_MASK(CSEL)


#define NFI_IOCON_REG16					0x094
#define   BRSTN_S					  4
#define   BRSTN_M					  0x0f
#define   L2NW_S					  2
#define   L2NW_M					  0x01
#define   L2NW						  REG_MASK(L2NW)
#define   L2NR_S					  1
#define   L2NR_M					  0x01
#define   L2NR						  REG_MASK(L2NR)
#define   NLD_PD_S					  0
#define   NLD_PD_M					  0x01
#define   NLD_PD					  REG_MASK(NLD_PD)


#define NFI_FDM0L_REG32					0x0a0
#define   FDM0_3_S					  24
#define   FDM0_3_M					  0xff
#define   FDM0_2_S					  16
#define   FDM0_2_M					  0xff
#define   FDM0_1_S					  8
#define   FDM0_1_M					  0xff
#define   FDM0_0_S					  0
#define   FDM0_0_M					  0xff
#define NFI_FDML_REG32(n)				(0x0a0 + ((n) << 3))


#define NFI_FDM0M_REG32					0x0a4
#define   FDM0_7_S					  24
#define   FDM0_7_M					  0xff
#define   FDM0_6_S					  16
#define   FDM0_6_M					  0xff
#define   FDM0_5_S					  8
#define   FDM0_5_M					  0xff
#define   FDM0_4_S					  0
#define   FDM0_4_M					  0xff
#define NFI_FDMM_REG32(n)				(0x0a4 + ((n) << 3))


#define NFI_LOCK_REG16					0x100
#define   LOCK_ON_S					  0
#define   LOCK_ON_M					  0x01
#define   LOCK_ON					  REG_MASK(LOCK_ON)


#define NFI_LOCKCON_REG32				0x104
#define   LOCK15_CS_S					  31
#define   LOCK15_CS_M					  0x01
#define   LOCK15_CS					  REG_MASK(LOCK15_CS)
#define   LOCK15_EN_S					  30
#define   LOCK15_EN_M					  0x01
#define   LOCK15_EN					  REG_MASK(LOCK15_EN)
#define   LOCK14_CS_S					  29
#define   LOCK14_CS_M					  0x01
#define   LOCK14_CS					  REG_MASK(LOCK14_CS)
#define   LOCK14_EN_S					  28
#define   LOCK14_EN_M					  0x01
#define   LOCK14_EN					  REG_MASK(LOCK14_EN)
#define   LOCK13_CS_S					  27
#define   LOCK13_CS_M					  0x01
#define   LOCK13_CS					  REG_MASK(LOCK13_CS)
#define   LOCK13_EN_S					  26
#define   LOCK13_EN_M					  0x01
#define   LOCK13_EN					  REG_MASK(LOCK13_EN)
#define   LOCK12_CS_S					  25
#define   LOCK12_CS_M					  0x01
#define   LOCK12_CS					  REG_MASK(LOCK12_CS)
#define   LOCK12_EN_S					  24
#define   LOCK12_EN_M					  0x01
#define   LOCK12_EN					  REG_MASK(LOCK12_EN)
#define   LOCK11_CS_S					  23
#define   LOCK11_CS_M					  0x01
#define   LOCK11_CS					  REG_MASK(LOCK11_CS)
#define   LOCK11_EN_S					  22
#define   LOCK11_EN_M					  0x01
#define   LOCK11_EN					  REG_MASK(LOCK11_EN)
#define   LOCK10_CS_S					  21
#define   LOCK10_CS_M					  0x01
#define   LOCK10_CS					  REG_MASK(LOCK10_CS)
#define   LOCK10_EN_S					  20
#define   LOCK10_EN_M					  0x01
#define   LOCK10_EN					  REG_MASK(LOCK10_EN)
#define   LOCK9_CS_S					  19
#define   LOCK9_CS_M					  0x01
#define   LOCK9_CS					  REG_MASK(LOCK9_CS)
#define   LOCK9_EN_S					  18
#define   LOCK9_EN_M					  0x01
#define   LOCK9_EN					  REG_MASK(LOCK9_EN)
#define   LOCK8_CS_S					  17
#define   LOCK8_CS_M					  0x01
#define   LOCK8_CS					  REG_MASK(LOCK8_CS)
#define   LOCK8_EN_S					  16
#define   LOCK8_EN_M					  0x01
#define   LOCK8_EN					  REG_MASK(LOCK8_EN)
#define   LOCK7_CS_S					  15
#define   LOCK7_CS_M					  0x01
#define   LOCK7_CS					  REG_MASK(LOCK7_CS)
#define   LOCK7_EN_S					  14
#define   LOCK7_EN_M					  0x01
#define   LOCK7_EN					  REG_MASK(LOCK7_EN)
#define   LOCK6_CS_S					  13
#define   LOCK6_CS_M					  0x01
#define   LOCK6_CS					  REG_MASK(LOCK6_CS)
#define   LOCK6_EN_S					  12
#define   LOCK6_EN_M					  0x01
#define   LOCK6_EN					  REG_MASK(LOCK6_EN)
#define   LOCK5_CS_S					  11
#define   LOCK5_CS_M					  0x01
#define   LOCK5_CS					  REG_MASK(LOCK5_CS)
#define   LOCK5_EN_S					  10
#define   LOCK5_EN_M					  0x01
#define   LOCK5_EN					  REG_MASK(LOCK5_EN)
#define   LOCK4_CS_S					  9
#define   LOCK4_CS_M					  0x01
#define   LOCK4_CS					  REG_MASK(LOCK4_CS)
#define   LOCK4_EN_S					  8
#define   LOCK4_EN_M					  0x01
#define   LOCK4_EN					  REG_MASK(LOCK4_EN)
#define   LOCK3_CS_S					  7
#define   LOCK3_CS_M					  0x01
#define   LOCK3_CS					  REG_MASK(LOCK3_CS)
#define   LOCK3_EN_S					  6
#define   LOCK3_EN_M					  0x01
#define   LOCK3_EN					  REG_MASK(LOCK3_EN)
#define   LOCK2_CS_S					  5
#define   LOCK2_CS_M					  0x01
#define   LOCK2_CS					  REG_MASK(LOCK2_CS)
#define   LOCK2_EN_S					  4
#define   LOCK2_EN_M					  0x01
#define   LOCK2_EN					  REG_MASK(LOCK2_EN)
#define   LOCK1_CS_S					  3
#define   LOCK1_CS_M					  0x01
#define   LOCK1_CS					  REG_MASK(LOCK1_CS)
#define   LOCK1_EN_S					  2
#define   LOCK1_EN_M					  0x01
#define   LOCK1_EN					  REG_MASK(LOCK1_EN)
#define   LOCK0_CS_S					  1
#define   LOCK0_CS_M					  0x01
#define   LOCK0_CS					  REG_MASK(LOCK0_CS)
#define   LOCK0_EN_S					  0
#define   LOCK0_EN_M					  0x01
#define   LOCK0_EN					  REG_MASK(LOCK0_EN)


#define NFI_LOCKANOB_REG16				0x108
#define   PROG_RADD_NOB_M				  0x7
#define   PROG_RADD_NOB_S				  12
#define   PROG_CADD_NOB_M				  0x3
#define   PROG_CADD_NOB_S				  8
#define   ERASE_RADD_NOB_M				  0x7
#define   ERASE_RADD_NOB_S				  4
#define   ERASE_CADD_NOB_M				  0x7
#define   ERASE_CADD_NOB_S				  0


#define NFI_LOCK00ADD_REG16				0x110
#define   LOCK0_ROW3_S					  24
#define   LOCK0_ROW3_M					  0xff
#define   LOCK0_ROW2_S					  16
#define   LOCK0_ROW2_M					  0xff
#define   LOCK0_ROW1_S					  8
#define   LOCK0_ROW1_M					  0xff
#define   LOCK0_ROW0_S					  0
#define   LOCK0_ROW0_M					  0xff


#define NFI_LOCK00FMT_REG16				0x114
#define   LOCK0_FMT3_S					  24
#define   LOCK0_FMT3_M					  0xff
#define   LOCK0_FMT2_S					  16
#define   LOCK0_FMT2_M					  0xff
#define   LOCK0_FMT1_S					  8
#define   LOCK0_FMT1_M					  0xff
#define   LOCK0_FMT0_S					  0
#define   LOCK0_FMT0_M					  0xff


#define NFI_FIFODATA0_REG32				0x190
#define NFI_FIFODATA1_REG32				0x194
#define NFI_FIFODATA2_REG32				0x198
#define NFI_FIFODATA3_REG32				0x19c


#define NFI_MCON_REG16					0x200
#define   BMCLR_S					  1
#define   BMCLR_M					  0x01
#define   BMCLR						  REG_MASK(BMCLR)
#define   BMSTR_S					  0
#define   BMSTR_M					  0x01
#define   BMSTR						  REG_MASK(BMSTR)


#define NFI_TOTALCNT_REG32				0x204
#define NFI_RQCNT_REG32					0x208
#define NFI_ACCNT_REG32					0x20c


#define NFI_MASTERSTA_REG16				0x210
#define   MAS_ADDR_S					  9
#define   MAS_ADDR_M					  0x07
#define   MAS_RD_S					  6
#define   MAS_RD_M					  0x07
#define   MAS_WR_S					  3
#define   MAS_WR_M					  0x07
#define   MAS_RDDLY_S					  0
#define   MAS_RDDLY_M					  0x07

typedef struct mt7621_nfc_sel
{
	struct nand_chip nand;
	int cs;

	u32 pagefmt_val;
	u32 acccon_val;

	u32 rowaddr_nob;
	u32 spare_per_sector;

	struct nand_ecclayout ecc_layout;

	void *page_cache;
	void *oob_mb_cache;

#ifdef CONFIG_NAND_MT7621_BBT_LAYER
	struct mtd_info *upper_mtd;
#endif
} mt7621_nfc_sel_t;

typedef struct mt7621_nfc
{
	void __iomem *nfi_base;
	void __iomem *ecc_base;

	mt7621_nfc_sel_t sels[CONFIG_SYS_NAND_MAX_CHIPS];

} mt7621_nfc_t;

typedef struct mt7621_nfc_timing
{
	u8 id[8];
	u32 id_len;
	u32 acccon_val;
} mt7621_nfc_timing_t;


#define NFI_DEFAULT_ACCESS_TIMING			0x30c77fff
#define NFI_ECC_SECTOR_SIZE				512
#define NFI_ECC_FDM_SIZE				8
#define NFI_ECC_PARITY_BITS				13

#define NFI_STATUS_WAIT_TIMEOUT_US			1000000


static mt7621_nfc_timing_t mt7621_nfc_def_timings[] =
{
	{{0xec, 0xd3, 0x51, 0x95, 0x58}, 5, 0x44333 }, /* K9K8G8000 */
};



static u16 __maybe_unused nfi_read16(mt7621_nfc_t *nfc, u32 addr)
{
	return readw(nfc->nfi_base + addr);
}

static void __maybe_unused nfi_write16(mt7621_nfc_t *nfc, u32 addr, u16 val)
{
	return writew(val, nfc->nfi_base + addr);
}

static void __maybe_unused nfi_setbits16(mt7621_nfc_t *nfc, u32 addr, u16 bits)
{
	u16 val = nfi_read16(nfc, addr);
	return writew(val | bits, nfc->nfi_base + addr);
}

static void __maybe_unused nfi_clrbits16(mt7621_nfc_t *nfc, u32 addr, u16 bits)
{
	u16 val = nfi_read16(nfc, addr);
	return writew(val & ~bits, nfc->nfi_base + addr);
}

static void __maybe_unused nfi_clrsetbits16(mt7621_nfc_t *nfc, u32 addr, u16 clr, u16 set)
{
	u16 val = nfi_read16(nfc, addr);
	return writew((val & ~clr) | set, nfc->nfi_base + addr);
}

static u32 __maybe_unused nfi_read32(mt7621_nfc_t *nfc, u32 addr)
{
	return readl(nfc->nfi_base + addr);
}

static void __maybe_unused nfi_write32(mt7621_nfc_t *nfc, u32 addr, u32 val)
{
	return writel(val, nfc->nfi_base + addr);
}

static int __maybe_unused nfc_wait_status_ready(mt7621_nfc_t *nfc, u32 status, const char *st_name)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout(nfc->nfi_base + NFI_STA_REG32, val,
		!(val & status), NFI_STATUS_WAIT_TIMEOUT_US);

	if (ret)
	{
		printf("Warning: NFI master timed out waiting for %s ready\n", st_name);
		return -EIO;
	}

	return 0;
}

static void __maybe_unused nfc_hw_reset(mt7621_nfc_t *nfc)
{
	u32 val, mask;
	int ret;

	mask = REG_MASK(MAS_ADDR) | REG_MASK(MAS_RD) | REG_MASK(MAS_WR) | REG_MASK(MAS_RDDLY);

	/* reset all registers and force the NFI master to terminate */
	nfi_write16(nfc, NFI_CON_REG16, NFI_FIFO_FLUSH | NFI_STM_RST);

	/* wait for the master to finish the last transaction */
	ret = readw_poll_timeout(nfc->nfi_base + NFI_MASTERSTA_REG16, val,
		!(val & mask), NFI_STATUS_WAIT_TIMEOUT_US);

	if (ret)
		printf("Warning: NFI master is still active after reset\n");

	/* ensure any status register affected by the NFI master is reset */
	nfi_write16(nfc, NFI_CON_REG16, NFI_FIFO_FLUSH | NFI_STM_RST);
	nfi_write16(nfc, NFI_STRDATA_REG16, 0);
}

static int __maybe_unused nfc_issue_cmd(mt7621_nfc_t *nfc, u8 cmd)
{
	nfi_write32(nfc, NFI_CMD_REG16, cmd);

	return nfc_wait_status_ready(nfc, STA_CMD, "command mode");
}

static int __maybe_unused nfc_issue_address(mt7621_nfc_t *nfc, u32 col_addr, u32 row_addr, u16 col_nob, u16 row_nob)
{
	nfi_write32(nfc, NFI_COLADDR_REG32, col_addr);
	nfi_write32(nfc, NFI_ROWADDR_REG32, row_addr);
	nfi_write16(nfc, NFI_ADDRNOB_REG16, REG_SET_VAL(ADDR_ROW_NOB, row_nob) | REG_SET_VAL(ADDR_COL_NOB, col_nob));

	return nfc_wait_status_ready(nfc, ADDR, "address mode");
}

static int __maybe_unused nfc_is_pio_ready(mt7621_nfc_t *nfc)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout(nfc->nfi_base + NFI_PIO_DIRDY_REG16, val,
		(val & PIO_DIRDY), NFI_STATUS_WAIT_TIMEOUT_US);

	if (ret)
	{
		printf("warning: NFI master PIO mode not ready\n");
		return -EIO;
	}

	return 0;
}

static void __maybe_unused nfc_read_oob(struct nand_chip *chip, void *out)
{
	mt7621_nfc_t *nfc = nand_get_controller_data(chip);
	u32 tmp, *p = (u32 *) out;
	u32 i;

	if ((size_t) out % sizeof(u32) == 0)
	{
		for (i = 0; i < chip->ecc.steps; i++)
		{
			*p++ = nfi_read32(nfc, NFI_FDML_REG32(i));
			*p++ = nfi_read32(nfc, NFI_FDMM_REG32(i));
		}
	}
	else
	{
		for (i = 0; i < chip->ecc.steps; i++)
		{
			tmp = nfi_read32(nfc, NFI_FDML_REG32(i));
			memcpy(p++, &tmp, 4);
			tmp = nfi_read32(nfc, NFI_FDMM_REG32(i));
			memcpy(p++, &tmp, 4);
		}
	}
}

static int __maybe_unused nfc_init_chip_config(mt7621_nfc_sel_t *nfc_sel)
{
	struct nand_chip *chip = &nfc_sel->nand;
	u32 spare_bytes;

	chip->ecc.size = NFI_ECC_SECTOR_SIZE;
	chip->ecc.steps = chip->mtd.writesize / chip->ecc.size;

	nfc_sel->spare_per_sector = chip->mtd.oobsize / chip->ecc.steps;

	if (nfc_sel->spare_per_sector >= 28)
	{
		spare_bytes = PAGEFMT_SPARE_28;
		chip->ecc.strength = 12;
		nfc_sel->spare_per_sector = 28;
	}
	else if (nfc_sel->spare_per_sector >= 27)
	{
		spare_bytes = PAGEFMT_SPARE_27;
		chip->ecc.strength = 8;
		nfc_sel->spare_per_sector = 27;
	}
	else if (nfc_sel->spare_per_sector >= 26)
	{
		spare_bytes = PAGEFMT_SPARE_26;
		chip->ecc.strength = 8;
		nfc_sel->spare_per_sector = 26;
	}
	else if (nfc_sel->spare_per_sector >= 16)
	{
		spare_bytes = PAGEFMT_SPARE_16;
		chip->ecc.strength = 4;
		nfc_sel->spare_per_sector = 16;
	}
	else
	{
		debug("Error: Not supported oob size %d\n", chip->mtd.oobsize);
		return -EINVAL;
	}

	chip->ecc.bytes = DIV_ROUND_UP(chip->ecc.strength * NFI_ECC_PARITY_BITS, 8);

	switch (chip->mtd.writesize)
	{
	case SZ_512:
		nfc_sel->pagefmt_val = REG_SET_VAL(PAGEFMT_SPARE, PAGEFMT_SPARE_16) | PAGEFMT_512;
		break;
	case SZ_2K:
		nfc_sel->pagefmt_val = REG_SET_VAL(PAGEFMT_SPARE, spare_bytes) | PAGEFMT_2K;
		break;
	case SZ_4K:
		nfc_sel->pagefmt_val = REG_SET_VAL(PAGEFMT_SPARE, spare_bytes) | PAGEFMT_4K;
		break;
	default:
		printf("Error: Not supported page size %d\n", chip->mtd.writesize);
		return -EINVAL;
	}

	nfc_sel->pagefmt_val |= REG_SET_VAL(PAGEFMT_FDM_ECC, NFI_ECC_FDM_SIZE) | REG_SET_VAL(PAGEFMT_FDM, NFI_ECC_FDM_SIZE);

	return 0;
}

static void __maybe_unused nfc_set_def_timing(mt7621_nfc_sel_t *nfc_sel, u8 *id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt7621_nfc_def_timings); i++)
	{
		if (!memcmp(mt7621_nfc_def_timings[i].id, id, mt7621_nfc_def_timings[i].id_len))
		{
			nfc_sel->acccon_val = mt7621_nfc_def_timings[i].acccon_val;
			return;
		}
	}

	nfc_sel->acccon_val = NFI_DEFAULT_ACCESS_TIMING;
}

extern void nfc_ecc_init(mt7621_nfc_t *nfc, struct nand_ecc_ctrl *ecc);
extern int nfc_ecc_decoder_wait_done(mt7621_nfc_t *nfc, u32 sector_index);
extern void nfc_ecc_encoder_start(mt7621_nfc_t *nfc);
extern void nfc_ecc_encoder_stop(mt7621_nfc_t *nfc);
extern void nfc_ecc_decoder_start(mt7621_nfc_t *nfc);
extern void nfc_ecc_decoder_stop(mt7621_nfc_t *nfc);
extern int nfc_ecc_correct_check(mt7621_nfc_t *nfc, mt7621_nfc_sel_t *nfc_sel, u8 *sector_buf, u32 sector_index);

#endif /* _MT7621_NAND_H_ */
