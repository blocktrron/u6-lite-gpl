/*
 * (C) Copyright 2018  Ubiquiti Networks
 *
 * SPDX-License-Identifier:<TAB>GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/addrspace.h>

#define GPIO_BANK(pin) ((pin) >> 5)
#define GPIO_PIN(pin) ((pin) & 0x1f)

#include <mach/mt7621_regs.h>
#define GPIO_CTRL(bank)	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_CTRL0_REG + bank * 4)
#define GPIO_DATA(bank)	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_DATA0_REG + bank * 4)
#define GPIO_DSET(bank)	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_DSET0_REG + bank * 4)
#define GPIO_DCLR(bank)	(void __iomem *) CKSEG1ADDR(MT7621_GPIO_BASE + MT7621_GPIO_DCLR0_REG + bank * 4)

static int mt7621_gpio_request(struct udevice *dev, unsigned offset, const char *label)
{
	return 0;
}

static int mt7621_gpio_direction_input(struct udevice *dev, unsigned offset)
{
	int bank = GPIO_BANK(offset);
	int pin = GPIO_PIN(offset);

	u32 d = readl(GPIO_CTRL(bank));
	d &= ~(0x1 << pin);
	writel(d, GPIO_CTRL(bank));

	return 0;
}

static int mt7621_gpio_get_value(struct udevice *dev, unsigned offset)
{
	int bank = GPIO_BANK(offset);
	int pin = GPIO_PIN(offset);

	return (readl(GPIO_DATA(bank)) >> pin ) & 0x1;
}

static int mt7621_gpio_set_value(struct udevice *dev, unsigned offset, int value)
{
	int bank = GPIO_BANK(offset);
	int pin = GPIO_PIN(offset);

	if (value) {
		writel(0x1 << pin, GPIO_DSET(bank));
	} else {
		writel(0x1 << pin, GPIO_DCLR(bank));
	}

	return 0;
}

static int mt7621_gpio_direction_output(struct udevice *dev, unsigned offset, int value)
{
	int bank = GPIO_BANK(offset);
	int pin = GPIO_PIN(offset);

	u32 d = readl(GPIO_CTRL(bank));
	d |= (0x1 << pin);
	writel(d, GPIO_CTRL(bank));
	mt7621_gpio_set_value(dev, offset, value);

	return 0;
}

static int mt7621_gpio_get_function(struct udevice *dev, unsigned offset)
{
	int bank = GPIO_BANK(offset);
	int pin = GPIO_PIN(offset);

	u32 d = readl(GPIO_CTRL(bank));
	int val = (d >> pin & 0x1);
	if (val)
		return GPIOF_OUTPUT;
	return GPIOF_INPUT;
}

static int mt7621_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->gpio_count = 96;
	uc_priv->bank_name = strdup("mt7621_");

	return 0;
}

static const struct dm_gpio_ops gpio_mt7621_ops = {
 	.request		= mt7621_gpio_request,
 	.direction_input	= mt7621_gpio_direction_input,
 	.direction_output	= mt7621_gpio_direction_output,
 	.get_value		= mt7621_gpio_get_value,
 	.set_value		= mt7621_gpio_set_value,
 	.get_function		= mt7621_gpio_get_function,
};

static const struct udevice_id mt7621_gpio_ids[] = {
	{ .compatible = "mediatek,mt7621-gpio" },
	{ }
};

U_BOOT_DRIVER(gpio_mt7621) = {
	.name	= "gpio_mt7621",
	.id	= UCLASS_GPIO,
	.of_match = mt7621_gpio_ids,
	.ops	= &gpio_mt7621_ops,
	.probe	= mt7621_gpio_probe,
};
