/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/addrspace.h>
#include <mach/mt7621_regs.h>
#include <dm.h>
#include <ns16550.h>
#include <dm/platform_data/mt7621_eth.h>
#include <phy.h>

int arch_early_init_r(void)
{
	void __iomem *base;
	u32 val;

	base = (void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE);

	/* Enable MIDO/RGMII1 function */
	clrbits_le32(base + MT7621_SYS_GPIO_MODE_REG,
		REG_MAKE_MASK(MDIO_MODE) | REG_MAKE_MASK(RGMII1_MODE));

	/* Reduce MDIO PAD driving strength */
	clrbits_le32(base + MT7621_SYS_PAD_RGMII2_MDIO_CFG_REG,
		REG_MAKE_MASK(MDIO_E4_E2));

	/* Reset Frame Engine/MCM/GSW */
	val = REG_MAKE_VAL(ETH_RST, 1) |
		REG_MAKE_VAL(FE_RST, 1) |
		REG_MAKE_VAL(MCM_RST, 1);

	setbits_le32(base + MT7621_SYS_RSTCTL_REG, val);
	udelay(200);
	clrbits_le32(base + MT7621_SYS_RSTCTL_REG, val);

	/* Reset Frame Engine SRAM */
	base = (void __iomem *) CKSEG1ADDR(MT7621_FE_BASE);
	setbits_le32(base + MT7621_FE_RST_GLO_REG,
		REG_MAKE_VAL(FE_PSE_RESET, 1));

	return 0;
}

#if !CONFIG_IS_ENABLED(OF_CONTROL) && defined(CONFIG_DM_SERIAL)
static const struct ns16550_platdata mt7621_uart_dev[] =
{
	{
		.base = CKSEG1ADDR(MT7621_UART1_BASE),
		.reg_shift = 2,
		.clock = CONFIG_SYS_NS16550_CLK,
		.fcr = UART_FCR_DEFVAL
	},
};

U_BOOT_DEVICE(mt7621_uart) =
{
	.name = "ns16550_serial",
	.platdata = &mt7621_uart_dev[0],
};
#endif

#if !CONFIG_IS_ENABLED(OF_CONTROL) && defined(CONFIG_DM_ETH)
static const struct mt7621_eth_pdata mt7621_eth_dev =
{
	.eth_pdata =
	{
		.iobase = CKSEG1ADDR(MT7621_FE_BASE),
		.phy_interface = PHY_INTERFACE_MODE_RGMII,
	},
	.fe_base = (void __iomem *) CKSEG1ADDR(MT7621_FE_BASE),
	.gmac_base = (void __iomem *) CKSEG1ADDR(MT7621_GMAC_BASE),
};

U_BOOT_DEVICE(mt7621_eth) =
{
	.name = "mt7621-eth",
	.platdata = &mt7621_eth_dev,
};
#endif
