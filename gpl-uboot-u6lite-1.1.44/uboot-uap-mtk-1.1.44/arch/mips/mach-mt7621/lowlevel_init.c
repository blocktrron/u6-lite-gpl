/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <div64.h>
#include <asm/io.h>
#include <asm/addrspace.h>
#include <asm/mipsregs.h>
#include <linux/sizes.h>
#include <mach/mt7621_regs.h>
#include <mach/mt7621_ddr_param.h>
#include "serial.h"
#ifdef CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_HW_RESET
#include "gpio.h"
#endif

DECLARE_GLOBAL_DATA_PTR;

extern void mt7621_cps_init(void);
extern void mt_dramc_init(mpll_ddr_config_t *pcfg);
extern void mt7621_get_clocks(u32 *pcpu_clk, u32 *pbus_clk, u32 *pxtal_clk);

extern void __weak board_get_ddr2_init_param(const ddr_ac_timing_param_t **, u32 *);
extern void __weak board_get_ddr3_init_param(const ddr_ac_timing_param_t **, u32 *);

static void mt7621_dram_init(void);
static int mt7621_dram_init_fail_recovery(void);
static void mt7621_xhci_config(void);

void lowlevel_init(void)
{
	void __iomem *base;

	/* Initialize Coherent Processing System (CPS) related components */
	mt7621_cps_init();

	/* Set SPI clock to system bus / (5 + 2) */
	base = (void __iomem *) CKSEG1ADDR(MT7621_SPI_BASE);
	clrsetbits_le32(base + MT7621_SPI_SPACE_REG, REG_MAKE_MASK(FS_CLK_SEL), REG_MAKE_VAL(FS_CLK_SEL, 5));

	/* Change CPU ratio from 1/0xA to 1/1 */
	base = (void __iomem *) CKSEG1ADDR(MT7621_RBUS_BASE);
	writel(REG_MAKE_VAL(CPU_FDIV, 1) | REG_MAKE_VAL(CPU_FFRAC, 1), base + MT7621_RBUS_DYN_CFG0_REG);

	/* Initialize early serial console */
	early_serial_init();

	/* Do DRAMC & DDR initialization */
	mt7621_dram_init();

	/* Change CPU PLL from 500MHz to CPU_PLL */
	base = (void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE);
	clrsetbits_le32(base + MT7621_SYS_CLKCFG0_REG, REG_MAKE_MASK(CPU_CLK_SEL), REG_MAKE_VAL(CPU_CLK_SEL, 1));
	get_cpu_freq(0);
	gd->arch.cpu_clk_stable = 1;

	/* Setup USB xHCI */
	mt7621_xhci_config();
}

#if defined(CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_SW_RESET)
/* Called during DDR initialization */
void mt7621_dramc_reset(void)
{
	void __iomem *base;

	base = (void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE);

	/* Reset DRAMC */
	setbits_le32(base + MT7621_SYS_RSTCTL_REG, REG_MAKE_VAL(MC_RST, 1));
	__udelay(CONFIG_MT7621_DRAM_RESET_DELAY);
	clrbits_le32(base + MT7621_SYS_RSTCTL_REG, REG_MAKE_VAL(MC_RST, 1));
}
#endif

static void mt7621_dram_init(void)
{
	void __iomem *base;
	mpll_ddr_config_t cfg;

	memset(&cfg, 0, sizeof (cfg));

#ifdef CONFIG_MT7621_DRAM_INIT_USER_PARAM_FIRST
#ifdef CONFIG_MT7621_DRAM_INIT_USER_PARAM_BOARD_PROVIDED
	if (board_get_ddr2_init_param)
		board_get_ddr2_init_param(&cfg.user_defined_ddr2_param, &cfg.user_expected_ddr2_size);

	if (board_get_ddr3_init_param)
		board_get_ddr3_init_param(&cfg.user_defined_ddr3_param, &cfg.user_expected_ddr3_size);

	if ((cfg.user_defined_ddr2_param && cfg.user_expected_ddr2_size) ||
		(cfg.user_defined_ddr3_param && &cfg.user_expected_ddr3_size))
		cfg.init_type = DDR_INIT_BOARD_PROVIDED_FIRST;
	else
		cfg.init_type = DDR_INIT_AUTO_PROBE;
#else /* ! CONFIG_MT7621_DRAM_INIT_USER_PARAM_BOARD_PROVIDED */
#ifdef CONFIG_MT7621_DRAM_DDR2_512M
	cfg.pre_defined_ddr2_param = DDR2_512M;
#elif defined (CONFIG_MT7621_DRAM_DDR2_512M_W9751G6KB_A02_1066MHZ)
	cfg.pre_defined_ddr2_param = DDR2_W9751G6KB_A02_1066_512M;
#elif defined (CONFIG_MT7621_DRAM_DDR2_1024M)
	cfg.pre_defined_ddr2_param = DDR2_1024M;
#elif defined (CONFIG_MT7621_DRAM_DDR2_1024M_W971GG6KB25_800MHZ)
	cfg.pre_defined_ddr2_param = DDR2_W971GG6KB25_800_1024M;
#elif defined (CONFIG_MT7621_DRAM_DDR2_1024M_W971GG6KB18_1066MHZ)
	cfg.pre_defined_ddr2_param = DDR2_W971GG6KB18_1066_1024M;
#else
#error Invalid pre-defined DDR2 parameter selection
#endif

#ifdef CONFIG_MT7621_DRAM_DDR3_1024M
	cfg.pre_defined_ddr3_param = DDR3_1024M;
#elif defined (CONFIG_MT7621_DRAM_DDR3_2048M)
	cfg.pre_defined_ddr3_param = DDR3_2048M;
#elif defined (CONFIG_MT7621_DRAM_DDR3_4096M)
	cfg.pre_defined_ddr3_param = DDR3_4096M;
#else
#error Invalid pre-defined DDR3 parameter selection
#endif

	cfg.init_type = DDR_INIT_PRE_DEFINED_FIRST;
#endif  /* CONFIG_MT7621_DRAM_INIT_USER_PARAM_BOARD_PROVIDED */
#else   /* ! CONFIG_MT7621_DRAM_INIT_USER_PARAM_FIRST */
	cfg.init_type = DDR_INIT_AUTO_PROBE;
#endif  /* CONFIG_MT7621_DRAM_INIT_USER_PARAM_FIRST */

	cfg.dram_speed = CONFIG_MT7621_DRAM_FREQ;
	cfg.cpu_speed = CONFIG_MT7621_CPU_FREQ;
	cfg.reinit = DDR_NOREINIT;

	printf("DRAM cfg init type(%d): %s\n", cfg.init_type,
	       (cfg.init_type == DDR_INIT_AUTO_PROBE) ? "auto probe" :
	       (cfg.init_type == DDR_INIT_PRE_DEFINED_FIRST) ?
	       "predefined first" : "board defined first");

	memset(cfg.invalid_memsizes, 0, sizeof(cfg.invalid_memsizes));
	cfg.num_invalid_memsizes = 0;

	/* Base clock used for MPLL */
	mt7621_get_clocks(NULL, NULL, &cfg.xtal_freq);

	/* DRAM type: 0 for DDR3, 1 for DDR2 */
	base = (void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE);
	cfg.dram_type = REG_GET_VAL(DRAM_TYPE, readl(base + MT7621_SYS_SYSCFG0_REG)) ? MT7621_DDR2 : MT7621_DDR3;

	/* Change MPLL source from Xtal to CR */
	setbits_le32(base + MT7621_SYS_CLKCFG0_REG, REG_MAKE_VAL(MPLL_CFG_SEL, 1));

	do
	{
		/* Reset DRAMC */
		setbits_le32(base + MT7621_SYS_RSTCTL_REG, REG_MAKE_VAL(MC_RST, 1));
		__udelay(CONFIG_MT7621_DRAM_RESET_DELAY);
		clrbits_le32(base + MT7621_SYS_RSTCTL_REG, REG_MAKE_VAL(MC_RST, 1));

		/* Initialization for DRAMC/DDR/CPU_PLL */
		mt_dramc_init(&cfg);
	} while (cfg.reinit != DDR_NOREINIT && cfg.reinit != DDR_REINIT_GIVE_UP);

	if (cfg.reinit != DDR_REINIT_GIVE_UP)
		gd->ram_size = cfg.expected_memsize;
	else {
		gd->ram_size = cfg.actual_memsize;

		printf("mt7621_dramc_init failed\n");
		if (mt7621_dram_init_fail_recovery()) {
			hang();
		}
	}
#ifdef CONFIG_UBNT_APP
#ifdef CONFIG_SYS_MEM_TOP_HIDE
	gd->ram_size -= CONFIG_SYS_MEM_TOP_HIDE;
#endif
#endif
}

/**
 *
 * mt7621_dram_init_fail_recovery -- try SW/HW reset CPU to recover DRAM
 *
 * Both of software and hardware reset triggers CPU reset.
 * If the device supports HW reset circuit. After HW reset, the CPU should
 * restart and won't enter SW reset and hang(). If not, the GPIO connects to
 * nothing, and code will be fallback to use SW reset. If neither HW reset nor SW
 * reset configured, code return -1 that will be fallback to MTK default hang()
 * function.
 */
static int mt7621_dram_init_fail_recovery(void)
{
#ifdef CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_HW_RESET
#ifndef CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_HW_RESET_GPIO
#error No GPIO defined for hard reset when DRAM initialization fails
#endif
	printf("Try to reset CPU by toggling HW reset GPIO%d to recover dram\n",
	       CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_HW_RESET_GPIO);

	/*
	 * Reset CPU by toggling hardware reset GPIO to recover the DRAM
	 * initialization failure. Hardware reset needs the hardware circuit
	 * that connects GPIO to reset IC MR(Manual Reset) pin.
	 */
	gpio_addr_setter(RT2880_REG_PIODIR,
			 CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_HW_RESET_GPIO,
			 GPIO_DIR_OUTPUT);
	gpio_output_set(CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_HW_RESET_GPIO, 0);
	udelay(100000);
	gpio_output_set(CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_HW_RESET_GPIO, 1);
#endif	/* CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_HW_RESET */

#ifdef CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_SW_RESET
	printf("Try soft reset CPU to recover dram\n");

	/*
	 * Reset CPU and DRAM through the chipset's reset control
	 * register to recover the DRAM initialization failure
	 */
	mt7621_dramc_reset();
	setbits_le32((void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE + MT7621_SYS_RSTCTL_REG),
		     REG_MAKE_MASK(SYS_RST));
#endif	/* CONFIG_UBNT_MT7621_DRAM_INIT_RECOVERY_SW_RESET */

	/* No way to recovery */
	return -1;
}

int dram_init(void)
{
#ifdef CONFIG_SKIP_LOWLEVEL_INIT
#ifdef CONFIG_BOARD_UBNT_UF_OLT
	// use only 256MB on OLT. Uboot crashes if we access memory beyond 256MB (e.g. in memmove() while relocating FDT)
	gd->ram_size = get_ram_size((void *) KSEG1, SZ_256M);
#else
	gd->ram_size = get_ram_size((void *) KSEG1, SZ_512M);
#endif
#endif
	return 0;
}

static void mt7621_xhci_config(void)
{
	void __iomem *base;

	base = (void __iomem *) CKSEG1ADDR(MT7621_SSUSB_BASE);

	switch (gd->arch.xtal_clk)
	{
	case 40 * 1000 * 1000:
		writel(0x10 |
			REG_MAKE_VAL(SSUSB_MAC3_SYS_CK_GATE_MASK_TIME, 0x20) |
			REG_MAKE_VAL(SSUSB_MAC2_SYS_CK_GATE_MASK_TIME, 0x20) |
			REG_MAKE_VAL(SSUSB_MAC3_SYS_CK_GATE_MODE, 2) |
			REG_MAKE_VAL(SSUSB_MAC2_SYS_CK_GATE_MODE, 2),
			base + MT7621_SSUSB_MAC_CK_CTRL_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_PREDIV_PE1D, 2) |
			REG_MAKE_VAL(SSUSB_PLL_PREDIV_U3, 1) |
			REG_MAKE_VAL(SSUSB_PLL_FBKDI, 4),
			base + MT7621_DA_SSUSB_U3PHYA_10_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_FBKDIV_PE2H, 0x18) |
			REG_MAKE_VAL(SSUSB_PLL_FBKDIV_PE1D, 0x18) |
			REG_MAKE_VAL(SSUSB_PLL_FBKDIV_PE1H, 0x18) |
			REG_MAKE_VAL(SSUSB_PLL_FBKDIV_U3, 0x1e),
			base + MT7621_DA_SSUSB_PLL_FBKDIV_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_PCW_NCPO_U3, 0x1e400000),
			base + MT7621_DA_SSUSB_PLL_PCW_NCPO_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_SSC_DELTA1_PE1H, 0x25) |
			REG_MAKE_VAL(SSUSB_PLL_SSC_DELTA1_U3, 0x73),
			base + MT7621_DA_SSUSB_PLL_SSC_DELTA1_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_SSC_DELTA_U3, 0x71) |
			REG_MAKE_VAL(SSUSB_PLL_SSC_DELTA1_PE2D, 0x4a),
			base + MT7621_DA_SSUSB_U3PHYA_21_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_SSC_PRD, 0x140),
			base + MT7621_SSUSB_U3PHYA_9_REG);

		writel(REG_MAKE_VAL(SSUSB_SYSPLL_PCW_NCPO, 0x11c00000),
			base + MT7621_SSUSB_U3PHYA_3_REG);

		writel(REG_MAKE_VAL(SSUSB_PCIE_CLKDRV_AMP, 4) |
			REG_MAKE_VAL(SSUSB_SYSPLL_FBSEL, 1) |
			REG_MAKE_VAL(SSUSB_SYSPLL_PREDIV, 1),
			base + MT7621_SSUSB_U3PHYA_1_REG);

		writel(REG_MAKE_VAL(SSUSB_SYSPLL_FBDIV, 0x12) |
			REG_MAKE_VAL(SSUSB_SYSPLL_VCO_DIV_SEL, 1) |
			REG_MAKE_VAL(SSUSB_SYSPLL_FPEN, 1) |
			REG_MAKE_VAL(SSUSB_SYSPLL_MONCK_EN, 1) |
			REG_MAKE_VAL(SSUSB_SYSPLL_VOD_EN, 1),
			base + MT7621_SSUSB_U3PHYA_2_REG);

		writel(REG_MAKE_VAL(SSUSB_EQ_CURSEL, 1) |
			REG_MAKE_VAL(SSUSB_RX_DAC_MUX, 8) |
			REG_MAKE_VAL(SSUSB_PCIE_SIGDET_VTH, 1) |
			REG_MAKE_VAL(SSUSB_PCIE_SIGDET_LPF, 1),
			base + MT7621_SSUSB_U3PHYA_11_REG);

		writel(REG_MAKE_VAL(SSUSB_RING_OSC_CNTEND, 0x1ff) |
			REG_MAKE_VAL(SSUSB_XTAL_OSC_CNTEND, 0x7f) |
			REG_MAKE_VAL(SSUSB_RING_BYPASS_DET, 1),
			base + MT7621_SSUSB_B2_ROSC_0_REG);

		writel(REG_MAKE_VAL(SSUSB_RING_OSC_FRC_RECAL, 3) |
			REG_MAKE_VAL(SSUSB_RING_OSC_FRC_SEL, 1),
			base + MT7621_SSUSB_B2_ROSC_1_REG);

		break;
	case 25 * 1000 * 1000:
		writel(0x10 |
			REG_MAKE_VAL(SSUSB_MAC3_SYS_CK_GATE_MASK_TIME, 0x20) |
			REG_MAKE_VAL(SSUSB_MAC2_SYS_CK_GATE_MASK_TIME, 0x20) |
			REG_MAKE_VAL(SSUSB_MAC3_SYS_CK_GATE_MODE, 2) |
			REG_MAKE_VAL(SSUSB_MAC2_SYS_CK_GATE_MODE, 2),
			base + MT7621_SSUSB_MAC_CK_CTRL_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_PREDIV_PE1D, 2) |
			REG_MAKE_VAL(SSUSB_PLL_FBKDI, 4),
			base + MT7621_DA_SSUSB_U3PHYA_10_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_FBKDIV_PE2H, 0x18) |
			REG_MAKE_VAL(SSUSB_PLL_FBKDIV_PE1D, 0x18) |
			REG_MAKE_VAL(SSUSB_PLL_FBKDIV_PE1H, 0x18) |
			REG_MAKE_VAL(SSUSB_PLL_FBKDIV_U3, 0x19),
			base + MT7621_DA_SSUSB_PLL_FBKDIV_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_PCW_NCPO_U3, 0x18000000),
			base + MT7621_DA_SSUSB_PLL_PCW_NCPO_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_SSC_DELTA1_PE1H, 0x25) |
			REG_MAKE_VAL(SSUSB_PLL_SSC_DELTA1_U3, 0x4a),
			base + MT7621_DA_SSUSB_PLL_SSC_DELTA1_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_SSC_DELTA_U3, 0x48) |
			REG_MAKE_VAL(SSUSB_PLL_SSC_DELTA1_PE2D, 0x4a),
			base + MT7621_DA_SSUSB_U3PHYA_21_REG);

		writel(REG_MAKE_VAL(SSUSB_PLL_SSC_PRD, 0x190),
			base + MT7621_SSUSB_U3PHYA_9_REG);

		writel(REG_MAKE_VAL(SSUSB_SYSPLL_PCW_NCPO, 0xe000000),
			base + MT7621_SSUSB_U3PHYA_3_REG);

		writel(REG_MAKE_VAL(SSUSB_PCIE_CLKDRV_AMP, 4) |
			REG_MAKE_VAL(SSUSB_SYSPLL_FBSEL, 1),
			base + MT7621_SSUSB_U3PHYA_1_REG);

		writel(REG_MAKE_VAL(SSUSB_SYSPLL_FBDIV, 0xf) |
			REG_MAKE_VAL(SSUSB_SYSPLL_VCO_DIV_SEL, 1) |
			REG_MAKE_VAL(SSUSB_SYSPLL_FPEN, 1) |
			REG_MAKE_VAL(SSUSB_SYSPLL_MONCK_EN, 1) |
			REG_MAKE_VAL(SSUSB_SYSPLL_VOD_EN, 1),
			base + MT7621_SSUSB_U3PHYA_2_REG);

		writel(REG_MAKE_VAL(SSUSB_EQ_CURSEL, 1) |
			REG_MAKE_VAL(SSUSB_RX_DAC_MUX, 8) |
			REG_MAKE_VAL(SSUSB_PCIE_SIGDET_VTH, 1) |
			REG_MAKE_VAL(SSUSB_PCIE_SIGDET_LPF, 1),
			base + MT7621_SSUSB_U3PHYA_11_REG);

		writel(REG_MAKE_VAL(SSUSB_RING_OSC_CNTEND, 0x1ff) |
			REG_MAKE_VAL(SSUSB_XTAL_OSC_CNTEND, 0x7f) |
			REG_MAKE_VAL(SSUSB_RING_BYPASS_DET, 1),
			base + MT7621_SSUSB_B2_ROSC_0_REG);

		writel(REG_MAKE_VAL(SSUSB_RING_OSC_FRC_RECAL, 3) |
			REG_MAKE_VAL(SSUSB_RING_OSC_FRC_SEL, 1),
			base + MT7621_SSUSB_B2_ROSC_1_REG);

		break;
	case 20 * 1000 * 1000:
		break;
	}
}
