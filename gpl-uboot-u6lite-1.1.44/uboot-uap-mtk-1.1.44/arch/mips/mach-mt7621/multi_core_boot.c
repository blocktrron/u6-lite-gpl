/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/addrspace.h>
#include <asm/mipsmtregs.h>
#include <asm/cm.h>
#include <mach/mt7621_regs.h>
#include "amon.h"

/* Cluster Power Controller (CPC) offsets */
#define CPC_CL_OTHER			0x2010
#define CPC_CO_CMD			0x4000

/* CPC_CL_OTHER fields */
#define CPC_CL_OTHER_CORENUM_SHIFT	16
#define CPC_CL_OTHER_CORENUM		GENMASK(23, 16)

/* CPC_CO_CMD */
#define PWR_UP				3


#define NUM_CPUS			4
#define WAIT_CPUS_TIMEOUT		4

extern char _start;

extern char amon_wait_code_start;
extern char amon_wait_code_end;

extern void join_coherent_domain(int ncores);
extern void boot_vpe1(void);

static void bootup_secondary_core(void);

static void copy_amon_wait_code(void)
{
	memset((void *)KSEG1, 0, 0x100);
	memset((void *)KSEG0, 0, 0x100);

	memcpy((void *)CMP_AMON_WAITCODE_IN_RAM,
		&amon_wait_code_start,
		&amon_wait_code_end - &amon_wait_code_start);
}

#ifndef CONFIG_SKIP_LOWLEVEL_INIT
void cpu_secondary_init_r(void)
#else
void cpu_secondary_init(void)
#endif
{
	void __iomem *base;
	u32  dual_core;
	cpulaunch_t *c;
	int i, cpuready = 1, cpumask = (1 << NUM_CPUS) - 1;
	u64 wait_tick;

	/* Copy AMON wait code used for other VPEs */
	copy_amon_wait_code();

	/* Bootup other core(s) */
	base = (void __iomem *) CKSEG1ADDR(MT7621_SYSCTL_BASE);
#ifndef CONFIG_MT7621_SINGLE_CORE_SUPPORT
	dual_core = REG_GET_VAL(CPU_ID, readl(base + MT7621_SYS_CHIP_REV_ID_REG));
#else
	dual_core = 0;
#endif

#ifdef CONFIG_MT7621_NAND_BOOT
	/* Make BootROM redirect Core1's bootup flow to our entry point */
	writel((ulong) &_start, base + MT7621_BOOT_SRAM_BASE_REG);
#endif

	/* Only bootup secondary core for MT7621A */
	if (dual_core)
		bootup_secondary_core();
	else
		cpumask = 0x03;

	/* Join the coherent domain */
	join_coherent_domain(dual_core ? 2 : 1);

	/* Bootup VPE1 */
	boot_vpe1();

	/* Wait for all CPU ready */
	wait_tick = get_ticks() + WAIT_CPUS_TIMEOUT * get_tbclk();

	while (1)
	{
		/* CPU0 is obviously ready */
		for (i = 1; i < 4; i++)
		{
			c = (cpulaunch_t *)(CKSEG0ADDR(CPULAUNCH) + (i << LOG2CPULAUNCH));
			if (c->flags & LAUNCH_FREADY)
			{
				if ((cpuready & BIT(i)) == 0)
				{
					debug("MIPS 1004K CPU%d is ready\n", i);
					cpuready |= BIT(i);
				}
			}
		}

		if ((cpuready & cpumask) == cpumask)
		{
			debug("MIPS 1004K All CPU ready\n");
			break;
		}

		if (get_ticks() > wait_tick)
		{
			puts("MIPS 1004K Warning: Not all CPU ready\n");
			break;
		}
	}
}

static void bootup_secondary_core(void)
{
	void __iomem *base;
	unsigned long num_cores;
	int i;

	num_cores = mips_cm_numcores();

	base = (void __iomem *) CKSEG1ADDR(MIPS_CPC_BASE);

	for (i = 1; i < num_cores; i++)
	{
		writel(i << CPC_CL_OTHER_CORENUM_SHIFT, base + CPC_CL_OTHER);
		writel(PWR_UP, base + CPC_CO_CMD);
	}
}
