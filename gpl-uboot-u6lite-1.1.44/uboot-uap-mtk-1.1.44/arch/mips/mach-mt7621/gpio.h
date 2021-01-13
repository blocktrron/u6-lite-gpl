// vim: ts=4:sw=4:expandtab
/* Copyright Ubiquti Networks Inc. 2019
** All Rights Reserved.
*/

#ifndef GPIO_H
#define GPIO_H

#include <mach/mt7621_regs.h>

enum gpio_addr_setter_mode
{
    GPIO_ADDR_SETTER_MODE_CLR,
    GPIO_ADDR_SETTER_MODE_SET,
    GPIO_ADDR_SETTER_MODE_ASSIGN
};

#define GPIO_DIR_INPUT   GPIO_ADDR_SETTER_MODE_CLR
#define GPIO_DIR_OUTPUT  GPIO_ADDR_SETTER_MODE_SET
#define RT2880_GPIOMODE_REG     (MT7621_GPIO_MODE_BASE)
#define RT2880_REG_PIODIR       (MT7621_GPIO_BASE + 0x0)
#define RT2880_REG_PIODATA      (MT7621_GPIO_BASE + 0x20)
#define RT2880_REG_PIOSET       (MT7621_GPIO_BASE + 0x30)
#define RT2880_REG_PIORESET     (MT7621_GPIO_BASE + 0x40)

#define ra_inl(offset)          (*(volatile unsigned long *)(offset))
#define ra_outl(offset,val)     (*(volatile unsigned long *)(offset) = val)
#define ra_and(addr, value)     ra_outl(addr, (ra_inl(addr) & (value)))
#define ra_or(addr, value)      ra_outl(addr, (ra_inl(addr) | (value)))

void gpio_addr_setter(uint32_t addr, int gpio_num, enum gpio_addr_setter_mode mode);
uint32_t gpio_addr_getter(uint32_t addr, int gpio_num);
int gpio_input_get(unsigned int gpio_num);
void gpio_output_set(unsigned int gpio_num, int status);

#endif /* GPIO_H */
