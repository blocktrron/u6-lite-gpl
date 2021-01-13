// vim: ts=4:sw=4:expandtab
/* Copyright Ubiquti Networks Inc. 2019
** All Rights Reserved.
*/

#include <common.h>
#include "gpio.h"

int
gpio_calculate_addr_bit(uint32_t *paddr, int gpio_num, uint32_t *pgpio_bit)
{
    if (gpio_num >= 96) {
        printf("ERROR: Support for gpio (%d) >= 96 not supported\n", gpio_num);
        return 1;
    }
    *paddr += ((gpio_num / 32) * 4);
    *pgpio_bit = 1 << (gpio_num % 32);
    return 0;
}

void
gpio_addr_setter(uint32_t addr, int gpio_num, enum gpio_addr_setter_mode mode)
{
    uint32_t gpio_bit;

    if (gpio_calculate_addr_bit(&addr, gpio_num, &gpio_bit))
        return;

    switch (mode) {
        case GPIO_ADDR_SETTER_MODE_CLR:
            ra_and(addr, ~gpio_bit);
            break;
        case GPIO_ADDR_SETTER_MODE_SET:
            ra_or(addr, gpio_bit);
            break;
        case GPIO_ADDR_SETTER_MODE_ASSIGN:
            ra_outl(addr, gpio_bit);
            break;
        default:
            printf("Unknown setter mode %d\n", mode);
            break;
    }
}

uint32_t
gpio_addr_getter(uint32_t addr, int gpio_num)
{
    uint32_t gpio_bit;
    if (gpio_calculate_addr_bit(&addr, gpio_num, &gpio_bit))
        return 0;
    return ra_inl(addr) & gpio_bit;
}

int gpio_input_get(unsigned int gpio_num)
{
    return gpio_addr_getter(RT2880_REG_PIODATA, gpio_num);
}

void gpio_output_set(unsigned int gpio_num, int status)
{
    if (status == 0)
        gpio_addr_setter(RT2880_REG_PIORESET, gpio_num, GPIO_ADDR_SETTER_MODE_ASSIGN);
    else
        gpio_addr_setter(RT2880_REG_PIOSET, gpio_num, GPIO_ADDR_SETTER_MODE_ASSIGN);
}
