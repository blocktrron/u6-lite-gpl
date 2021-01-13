/*
 * MT7621 I2C driver adapted from Linux driver (taken from LEDE project patches),
 * originally created by:
 *
 * Copyright (C) 2013 Steven Liu <steven_liu@mediatek.com>
 * Copyright (C) 2016 Michael Lee <igvtee@gmail.com>
 *
 * Improve driver for i2cdetect from i2c-tools to detect i2c devices on the bus.
 * (C) 2014 Sittisak <sittisaks@hotmail.com>
 *
 * Copyright (C) 2019 Ubiquiti Networks
 *
 * SPDX-License-Identifier:<TAB>GPL-2.0
*/

#include <common.h>
#include <dm.h>
#include <i2c.h>
#include <asm/io.h>
#include <wait_bit.h>
#include <clk.h>

#define REG_SM0CFG0		0x08
#define REG_SM0DOUT		0x10
#define REG_SM0DIN		0x14
#define REG_SM0ST		0x18
#define REG_SM0AUTO		0x1C
#define REG_SM0CFG1		0x20
#define REG_SM0CFG2		0x28
#define REG_SM0CTL0		0x40
#define REG_SM0CTL1		0x44
#define REG_SM0D0		0x50
#define REG_SM0D1		0x54
#define REG_PINTEN		0x5C
#define REG_PINTST		0x60
#define REG_PINTCL		0x64

/* REG_SM0CFG0 */
#define I2C_DEVADDR_MASK	0x7f

/* REG_SM0ST */
#define I2C_DATARDY		BIT(2)
#define I2C_SDOEMPTY		BIT(1)
#define I2C_BUSY		BIT(0)

/* REG_SM0AUTO */
#define READ_CMD		BIT(0)

/* REG_SM0CFG1 */
#define BYTECNT_MAX		64
#define SET_BYTECNT(x)		(x - 1)

/* REG_SM0CFG2 */
#define AUTOMODE_EN		BIT(0)

/* REG_SM0CTL0 */
#define ODRAIN_HIGH_SM0		BIT(31)
#define VSYNC_SHIFT		28
#define VSYNC_MASK		0x3
#define VSYNC_PULSE		(0x1 << VSYNC_SHIFT)
#define VSYNC_RISING		(0x2 << VSYNC_SHIFT)
#define CLK_DIV_SHIFT		16
#define CLK_DIV_MASK		0xfff
#define DEG_CNT_SHIFT		8
#define DEG_CNT_MASK		0xff
#define WAIT_HIGH		BIT(6)
#define DEG_EN			BIT(5)
#define CS_STATUA		BIT(4)
#define SCL_STATUS		BIT(3)
#define SDA_STATUS		BIT(2)
#define SM0_EN			BIT(1)
#define SCL_STRECH		BIT(0)

/* REG_SM0CTL1 */
#define ACK_SHIFT		16
#define ACK_MASK		0xff
#define PGLEN_SHIFT		8
#define PGLEN_MASK		0x7
#define SM0_MODE_SHIFT		4
#define SM0_MODE_MASK		0x7
#define SM0_MODE_START		0x1
#define SM0_MODE_WRITE		0x2
#define SM0_MODE_STOP		0x3
#define SM0_MODE_READ_NACK	0x4
#define SM0_MODE_READ_ACK	0x5
#define SM0_TRI_BUSY		BIT(0)

/* timeout waiting for I2C devices to respond (clock streching) */
#define TIMEOUT_MS              1000
#define DELAY_INTERVAL_US       100

#include <mach/mt7621_regs.h>
#define I2CREG(reg)	(void __iomem *) CKSEG1ADDR(MT7621_I2C_BASE + reg)

struct mtk_i2c {
	struct clk clk;
	u32 cur_clk;
	u32 clk_div;
};

static void mtk_i2c_w32(struct mtk_i2c *i2c, u32 val, unsigned reg)
{
	writel(val, I2CREG(reg));
}

static u32 mtk_i2c_r32(struct mtk_i2c *i2c, unsigned reg)
{
	return readl(I2CREG(reg));
}

static int mtk_i2c_wait_idle(struct mtk_i2c *i2c)
{
	int ret;

	ret = wait_for_bit_le32(I2CREG(REG_SM0ST),I2C_BUSY, 0, TIMEOUT_MS, 0);
	if (ret < 0)
		printf("idle err(%d)\n", ret);

	return ret;
}

static int mtk_i2c_wait_rx_done(struct mtk_i2c *i2c)
{
	int ret;

	ret = wait_for_bit_le32(I2CREG(REG_SM0ST), I2C_DATARDY, 1, TIMEOUT_MS, 0);
	if (ret < 0)
		printf("rx err(%d)\n", ret);

	return ret;
}

static int mtk_i2c_wait_tx_done(struct mtk_i2c *i2c)
{
	int ret;

	ret = wait_for_bit_le32(I2CREG(REG_SM0ST), I2C_SDOEMPTY, 1, TIMEOUT_MS, 0);
	if (ret < 0)
		printf("tx err(%d)\n", ret);

	return ret;
}

static void mtk_i2c_reset(struct mtk_i2c *i2c)
{
	u32 reg;

	/* ctrl0 */
	reg = ODRAIN_HIGH_SM0 | VSYNC_PULSE | (i2c->clk_div << CLK_DIV_SHIFT) |
		WAIT_HIGH | SM0_EN;
	mtk_i2c_w32(i2c, reg, REG_SM0CTL0);

	/* auto mode */
	mtk_i2c_w32(i2c, AUTOMODE_EN, REG_SM0CFG2);
}

static void mtk_i2c_dump_reg(struct mtk_i2c *i2c)
{
	printf("cfg0 %08x, dout %08x, din %08x, " \
			"status %08x, auto %08x, cfg1 %08x, " \
			"cfg2 %08x, ctl0 %08x, ctl1 %08x\n",
			mtk_i2c_r32(i2c, REG_SM0CFG0),
			mtk_i2c_r32(i2c, REG_SM0DOUT),
			mtk_i2c_r32(i2c, REG_SM0DIN),
			mtk_i2c_r32(i2c, REG_SM0ST),
			mtk_i2c_r32(i2c, REG_SM0AUTO),
			mtk_i2c_r32(i2c, REG_SM0CFG1),
			mtk_i2c_r32(i2c, REG_SM0CFG2),
			mtk_i2c_r32(i2c, REG_SM0CTL0),
			mtk_i2c_r32(i2c, REG_SM0CTL1));
}

static int mt7621_i2c_xfer(struct udevice *dev, struct i2c_msg *msgs,
				  int num)
{
	struct mtk_i2c *i2c;
	struct i2c_msg *pmsg;
	int i, j, ret;
	u32 cmd;

	i2c = dev_get_priv(dev);

	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];
		cmd = 0;

		/* wait hardware idle */
		if ((ret = mtk_i2c_wait_idle(i2c)))
			goto err_timeout;

		if (pmsg->flags & I2C_M_TEN) {
			dev_dbg(i2c->dev, "10 bits addr not supported\n");
			return -EINVAL;
		} else {
			/* 7 bits address */
			mtk_i2c_w32(i2c, pmsg->addr & I2C_DEVADDR_MASK,
					REG_SM0CFG0);
		}

		/* buffer length */
		if (pmsg->len == 0) {
			return -EINVAL;
		} else
			mtk_i2c_w32(i2c, SET_BYTECNT(pmsg->len),
					REG_SM0CFG1);

		j = 0;
		if (pmsg->flags & I2C_M_RD) {
			cmd |= READ_CMD;
			/* start transfer */
			barrier();
			mtk_i2c_w32(i2c, cmd, REG_SM0AUTO);
			do {
				/* wait */
				if ((ret = mtk_i2c_wait_rx_done(i2c))) {
					goto err_timeout;
				}
				/* read data */
				if (pmsg->len)
					pmsg->buf[j] = mtk_i2c_r32(i2c,
							REG_SM0DIN);
				mtk_i2c_w32(i2c, 0, REG_SM0ST);
				j++;
			} while (j < pmsg->len);
		} else {
			do {
				/* write data */
				if (pmsg->len)
					mtk_i2c_w32(i2c, pmsg->buf[j],
							REG_SM0DOUT);
				/* start transfer */
				if (j == 0) {
					barrier();
					mtk_i2c_w32(i2c, cmd, REG_SM0AUTO);
				}
				/* wait */
				if ((ret = mtk_i2c_wait_tx_done(i2c))) {
					goto err_timeout;
				}
				j++;
			} while (j < pmsg->len);
		}
	}
	return 0;

err_timeout:
	mtk_i2c_dump_reg(i2c);
	mtk_i2c_reset(i2c);
	return ret;
}

static int mt7621_i2c_set_bus_speed(struct udevice *dev, unsigned int speed)
{
	struct mtk_i2c *i2c = dev_get_priv(dev);

	i2c->cur_clk = speed;
	i2c->clk_div = clk_get_rate(&i2c->clk) / i2c->cur_clk;
	if (i2c->clk_div > CLK_DIV_MASK)
		i2c->clk_div = CLK_DIV_MASK;
	dev_dbg(i2c->dev, "i2c-mt7621 clock %uKHz\n", i2c->cur_clk/1000);

	mtk_i2c_reset(i2c);
	return 0;
}

static int mt7621_i2c_probe(struct udevice *dev)
{
	struct mtk_i2c *i2c = dev_get_priv(dev);
	int ret = 0;

#ifdef DEBUG
	u32 mode = readl((void __iomem *) CKSEG1ADDR(MT7621_GPIO_MODE_BASE));
	printf("mux: 0x%x (%s)\n", mode, mode & (1 << 2) ? "gpio" : "i2c");
#endif

	ret = clk_get_by_index(dev, 0, &i2c->clk);
	if (ret) {
		dev_err(dev, "i2c_mt7621: failed to get clock\n");
		return ret;
	}
	return ret;
}

static const struct dm_i2c_ops mt7621_i2c_ops = {
	.xfer = mt7621_i2c_xfer,
	.set_bus_speed = mt7621_i2c_set_bus_speed,
};

static const struct udevice_id mt7621_i2c_ids[] = {
	{ .compatible = "mediatek,mt7621-i2c" },
	{ }
};

U_BOOT_DRIVER(i2c_mt7621) = {
	.name	= "mt7621_i2c",
	.id	= UCLASS_I2C,
	.of_match = mt7621_i2c_ids,
	.probe	= mt7621_i2c_probe,
	.priv_auto_alloc_size = sizeof(struct mtk_i2c),
	.ops	= &mt7621_i2c_ops,
};
