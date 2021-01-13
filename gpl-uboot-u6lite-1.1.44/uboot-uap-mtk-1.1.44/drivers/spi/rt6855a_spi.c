/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spi.h>
#include <dm.h>
#include <div64.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/addrspace.h>
#include <asm/types.h>
#include <dm/pinctrl.h>

#if defined(CONFIG_MACH_MT7621)
#include <mach/mt7621_regs.h>

#define	GPIO_CTRL0	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_CTRL0_REG)
#define GPIO_DATA0	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_DATA0_REG)
#define GPIO_DSET0	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_DSET0_REG)
#define GPIO_DCLR0	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_DCLR0_REG)

#define	GPIO_CTRL	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_CTRL1_REG)
#define GPIO_DATA	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_DATA1_REG)
#define GPIO_DSET	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_DSET1_REG)
#define GPIO_DCLR	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_DCLR1_REG)

#ifdef CONFIG_BOARD_UBNT_UF_OLT
#define GPIO_CS0_MASK	0x00000008
#define GPIO0_CS0_MASK	0x00000040
#else
#define GPIO_CS0_MASK	0x00000004
#endif
#define GPIO_CS1_MASK	0x00000008
#define GPIO_MOSI_MASK	0x00000040
#define GPIO_MISO_MASK	0x00000020
#define GPIO_CLK_MASK	0x00000010

#define GPIO_MISO_SHIFT	5

#ifdef CONFIG_BOARD_UBNT_UF_OLT
#define SPI_CLOCK_SLOWDOWN_DELAY() udelay(1)
#else
#define SPI_CLOCK_SLOWDOWN_DELAY() (void)0 // make a semicolon requirement
#endif

static inline void rt6855a_spi_gpio_enable(void)
{
	clrsetbits_le32((void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE + MT7621_SYS_GPIO_MODE_REG),
		REG_MAKE_MASK(SPI_MODE), REG_MAKE_VAL(SPI_MODE, 1));
}

static inline void rt6855a_spi_gpio_disable(void)
{
#ifdef CONFIG_BOARD_UBNT_UF_OLT
	// switch back to NAND mode
	clrsetbits_le32((void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE + MT7621_SYS_GPIO_MODE_REG),
		REG_MAKE_MASK(SPI_MODE), REG_MAKE_VAL(SPI_MODE, 2));
#else
	clrbits_le32((void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE + MT7621_SYS_GPIO_MODE_REG),
		REG_MAKE_MASK(SPI_MODE));
#endif
}
#else
#error Invalid target, MT7621/MT7628/MT7688 required!
#endif

#define READ_ONE_BIT_MODE_0_3(res, bit) \
	writel(GPIO_CLK_MASK, GPIO_DSET); \
	(res) |= ((readl(GPIO_DATA) & GPIO_MISO_MASK) >> GPIO_MISO_SHIFT) << (bit); \
	writel(GPIO_CLK_MASK, GPIO_DCLR);

#define READ_ONE_BYTE_MODE_0_3(res, byte_order) \
	READ_ONE_BIT_MODE_0_3(res, 7 + 8 * (byte_order)); \
	READ_ONE_BIT_MODE_0_3(res, 6 + 8 * (byte_order)); \
	READ_ONE_BIT_MODE_0_3(res, 5 + 8 * (byte_order)); \
	READ_ONE_BIT_MODE_0_3(res, 4 + 8 * (byte_order)); \
	READ_ONE_BIT_MODE_0_3(res, 3 + 8 * (byte_order)); \
	READ_ONE_BIT_MODE_0_3(res, 2 + 8 * (byte_order)); \
	READ_ONE_BIT_MODE_0_3(res, 1 + 8 * (byte_order)); \
	READ_ONE_BIT_MODE_0_3(res, 0 + 8 * (byte_order));

#define WRITE_ONE_BIT_MODE_0_3(data, bit) \
	writel(GPIO_MOSI_MASK, GPIO_DCLR - ((((data) >> (bit)) & 1) << 4)); \
	writel(GPIO_CLK_MASK, GPIO_DSET); \
	SPI_CLOCK_SLOWDOWN_DELAY(); \
	writel(GPIO_CLK_MASK, GPIO_DCLR); \
	SPI_CLOCK_SLOWDOWN_DELAY(); \

#define WRITE_ONE_BYTE_MODE_0_3(data, byte_order) \
	WRITE_ONE_BIT_MODE_0_3((data), 7 + 8 * (byte_order)); \
	WRITE_ONE_BIT_MODE_0_3((data), 6 + 8 * (byte_order)); \
	WRITE_ONE_BIT_MODE_0_3((data), 5 + 8 * (byte_order)); \
	WRITE_ONE_BIT_MODE_0_3((data), 4 + 8 * (byte_order)); \
	WRITE_ONE_BIT_MODE_0_3((data), 3 + 8 * (byte_order)); \
	WRITE_ONE_BIT_MODE_0_3((data), 2 + 8 * (byte_order)); \
	WRITE_ONE_BIT_MODE_0_3((data), 1 + 8 * (byte_order)); \
	WRITE_ONE_BIT_MODE_0_3((data), 0 + 8 * (byte_order));

#define RW_ONE_BIT(res, data, bit, clk0, clk1) \
	writel(GPIO_MOSI_MASK, GPIO_DCLR - ((((data) >> (bit)) & 1) << 4)); \
	writel(GPIO_CLK_MASK, clk0); \
	(res) |= ((readl(GPIO_DATA) & GPIO_MISO_MASK) >> GPIO_MISO_SHIFT) << (bit); \
	SPI_CLOCK_SLOWDOWN_DELAY(); \
	writel(GPIO_CLK_MASK, clk1); \
	SPI_CLOCK_SLOWDOWN_DELAY();

#define RW_ONE_BYTE(res, data, byte_order, clk0, clk1) \
	RW_ONE_BIT((res), (data), 7 + 8 * (byte_order), clk0, clk1); \
	RW_ONE_BIT((res), (data), 6 + 8 * (byte_order), clk0, clk1); \
	RW_ONE_BIT((res), (data), 5 + 8 * (byte_order), clk0, clk1); \
	RW_ONE_BIT((res), (data), 4 + 8 * (byte_order), clk0, clk1); \
	RW_ONE_BIT((res), (data), 3 + 8 * (byte_order), clk0, clk1); \
	RW_ONE_BIT((res), (data), 2 + 8 * (byte_order), clk0, clk1); \
	RW_ONE_BIT((res), (data), 1 + 8 * (byte_order), clk0, clk1); \
	RW_ONE_BIT((res), (data), 0 + 8 * (byte_order), clk0, clk1);

struct rt6855a_spi_priv
{
	u32 cur_clk[2];		/* Per-CS */
};

static void __iomem *clk_dregs[2] = { GPIO_DCLR, GPIO_DSET };
static u32 cs_masks[2] = { GPIO_CS0_MASK, GPIO_CS1_MASK };

static void spi_cs_activate(struct udevice *dev, u32 cs, uint mode)
{
	struct udevice *bus = dev_get_parent(dev);
	struct rt6855a_spi_priv *priv = dev_get_priv(bus);

	rt6855a_spi_gpio_enable();

#ifdef CONFIG_BOARD_UBNT_UF_OLT
	if (cs == 0) {
		clrsetbits_le32(GPIO_CTRL0, 0, GPIO0_CS0_MASK); // output direction
		writel(GPIO0_CS0_MASK, GPIO_DCLR0);        // zero-out
	}
	clrsetbits_le32(GPIO_CTRL, GPIO_MISO_MASK, GPIO_CLK_MASK | cs_masks[cs] | GPIO_MOSI_MASK);
#else
	clrsetbits_32(GPIO_CTRL, GPIO_MISO_MASK, GPIO_CLK_MASK | cs_masks[cs] | GPIO_MOSI_MASK);
#endif
	priv->cur_clk[cs] = !!(mode & SPI_CPOL);

	writel(GPIO_CLK_MASK, clk_dregs[priv->cur_clk[cs]]);

	if (mode & SPI_CS_HIGH)
		writel(cs_masks[cs], GPIO_DSET);
	else
		writel(cs_masks[cs], GPIO_DCLR);

	if (mode & SPI_CPHA)
	{
		priv->cur_clk[cs] = !priv->cur_clk[cs];
		writel(GPIO_CLK_MASK, clk_dregs[priv->cur_clk[cs]]);
	}
}

static void spi_cs_deactivate(struct udevice *dev, u32 cs, uint mode)
{
	if (mode & SPI_CPHA)
		writel(GPIO_CLK_MASK, clk_dregs[!!(mode & SPI_CPOL)]);

	if (mode & SPI_CS_HIGH)
		writel(cs_masks[cs], GPIO_DCLR);
	else
		writel(cs_masks[cs], GPIO_DSET);

#ifdef CONFIG_BOARD_UBNT_UF_OLT
	if (cs == 0) {
		writel(GPIO0_CS0_MASK, GPIO_DSET0);
	}
#endif

	rt6855a_spi_gpio_disable();
}

static int rt6855a_spi_claim_bus(struct udevice *dev)
{
	return 0;
}

static int rt6855a_spi_release_bus(struct udevice *dev)
{
	return 0;
}

#ifdef CONFIG_RT6855A_SPI_FLASH_OPTIMIZATION
static void rt6855a_spi_byte_read_mode_0_3(unsigned int bytes, void *din)
{
	u8 *rx = (u8 *)din;
	u32 *rxw;
	u32 rd, ubytes;

	ubytes = ((size_t)rx) & (sizeof(u32) - 1);

	bytes -= ubytes;

	while (ubytes > 0)
	{
		ubytes--;
		rd = 0;

		READ_ONE_BYTE_MODE_0_3(rd, 0);

		*rx++ = rd;
	}

	rxw = (u32 *)rx;

	while (bytes >= sizeof(u32))
	{
		bytes -= sizeof(u32);
		rd = 0;

		READ_ONE_BYTE_MODE_0_3(rd, 0);
		READ_ONE_BYTE_MODE_0_3(rd, 1);
		READ_ONE_BYTE_MODE_0_3(rd, 2);
		READ_ONE_BYTE_MODE_0_3(rd, 3);

		*rxw++ = rd;
	}

	rx = (u8 *)rxw;

	while (bytes > 0)
	{
		bytes--;
		rd = 0;

		READ_ONE_BYTE_MODE_0_3(rd, 0);

		*rx++ = rd;
	}
}

static void rt6855a_spi_byte_write_mode_0_3(unsigned int bytes, const void *dout)
{
	const u8 *tx = (const u8 *)dout;
	const u32 *txw;
	u32 wr, ubytes;

	ubytes = ((size_t)tx) & (sizeof(u32) - 1);

	bytes -= ubytes;

	while (ubytes > 0)
	{
		ubytes--;
		wr = *tx++;

		WRITE_ONE_BYTE_MODE_0_3(wr, 0);
	}

	txw = (u32 *)tx;

	while (bytes >= sizeof(u32))
	{
		bytes -= sizeof(u32);
		wr = *txw++;

		WRITE_ONE_BYTE_MODE_0_3(wr, 0);
		WRITE_ONE_BYTE_MODE_0_3(wr, 1);
		WRITE_ONE_BYTE_MODE_0_3(wr, 2);
		WRITE_ONE_BYTE_MODE_0_3(wr, 3);
	}

	tx = (u8 *)txw;

	while (bytes > 0)
	{
		bytes--;
		wr = *tx++;

		WRITE_ONE_BYTE_MODE_0_3(wr, 0);
	}
}
#endif

static void rt6855a_spi_bit_readwrite(unsigned int bitlen, void *din, const void *dout, u32 cur_clk)
{
	u8 *rx = (u8 *) din;
	const u8 *tx = (const u8 *) dout;
	u32 bytes = bitlen / 8;
	u32 rd, wr, bit, restbits;
	void __iomem *clk_dreg_1, __iomem *clk_dreg_2;

	clk_dreg_1 = clk_dregs[!cur_clk];
	clk_dreg_2 = clk_dregs[cur_clk];

	while (bytes > 0)
	{
		bytes--;
		rd = 0;
		wr = *tx++;

		RW_ONE_BYTE(rd, wr, 0, clk_dreg_1, clk_dreg_2);

		*rx++ = rd;
	}

	rd = 0;
	wr = *tx++;
	restbits = (bitlen % 8);

	for (bit = 7; bit >= 8 - restbits; bit--)
	{
		RW_ONE_BIT(rd, wr, bit, clk_dreg_1, clk_dreg_2);
	}

	*rx++ = rd;
}

static int rt6855a_spi_xfer(struct udevice *dev, unsigned int bitlen,
		const void *dout, void *din, unsigned long flags)
{
	struct udevice *bus = dev_get_parent(dev);
	struct rt6855a_spi_priv *priv = dev_get_priv(bus);
	struct dm_spi_slave_platdata *slave = dev_get_parent_platdata(dev);

	if (flags & SPI_XFER_BEGIN)
		spi_cs_activate(dev, slave->cs, slave->mode);

#ifdef CONFIG_RT6855A_SPI_FLASH_OPTIMIZATION
	if (bitlen % 8 == 0)
	{
		if (((slave->mode & SPI_MODE_3) == 0) ||
			((slave->mode & SPI_MODE_3) == SPI_MODE_3))
		{
			u32 bytes = bitlen / 8;

			if (din && !dout)
			{
				rt6855a_spi_byte_read_mode_0_3(bytes, din);
				goto xfer_done;
			}

			if (!din && dout)
			{
				rt6855a_spi_byte_write_mode_0_3(bytes, dout);
				goto xfer_done;
			}
		}
	}
#endif

	rt6855a_spi_bit_readwrite(bitlen, din, dout, priv->cur_clk[slave->cs]);

#ifdef CONFIG_RT6855A_SPI_FLASH_OPTIMIZATION
xfer_done:
#endif

	if (flags & SPI_XFER_END)
		spi_cs_deactivate(dev, slave->cs, slave->mode);

	return 0;
}

static int rt6855a_spi_set_speed(struct udevice *bus, uint speed)
{
	return 0;
}

static int rt6855a_spi_set_mode(struct udevice *bus, uint mode)
{
	return 0;
}

static int rt6855a_spi_probe(struct udevice *bus)
{
	return 0;
}

static int rt6855a_cs_info(struct udevice *bus, uint cs,
			   struct spi_cs_info *info)
{
	if (cs >= 2)
		return -ENODEV;

	return 0;
}

static const struct dm_spi_ops rt6855a_spi_ops =
{
	.claim_bus  = rt6855a_spi_claim_bus,
	.release_bus    = rt6855a_spi_release_bus,
	.xfer       = rt6855a_spi_xfer,
	.set_speed  = rt6855a_spi_set_speed,
	.set_mode   = rt6855a_spi_set_mode,
	.cs_info    = rt6855a_cs_info,
};

#if CONFIG_IS_ENABLED(OF_CONTROL) && !CONFIG_IS_ENABLED(OF_PLATDATA)
static const struct udevice_id rt6855a_spi_ids[] =
{
	{ .compatible = "mediatek,rt6855a-spi" },
	{}
};
#endif

U_BOOT_DRIVER(rt6855a_spi) =
{
	.name   = "rt6855a_spi",
	.id = UCLASS_SPI,
#if CONFIG_IS_ENABLED(OF_CONTROL) && !CONFIG_IS_ENABLED(OF_PLATDATA)
	.of_match = rt6855a_spi_ids,
#endif
	.ops    = &rt6855a_spi_ops,
	.priv_auto_alloc_size = sizeof(struct rt6855a_spi_priv),
	.probe  = rt6855a_spi_probe,
};
