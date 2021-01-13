/*
 *  Startup Code for MediaTek MT7621 SPL
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm-offsets.h>
#include <config.h>
#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/addrspace.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/cm.h>
#include <asm/spl.h>

#undef CONFIG_SYS_INIT_SP_ADDR
#define CONFIG_SYS_INIT_SP_ADDR	0xbe10d000

	.set noreorder

	.macro init_wr sel
	MTC0	zero, CP0_WATCHLO,\sel
	mtc0	t1, CP0_WATCHHI,\sel
	mfc0	t0, CP0_WATCHHI,\sel
	bgez	t0, wr_done
	 nop
	.endm

	.macro uhi_mips_exception
	move	k0, t9		# preserve t9 in k0
	move	k1, a0		# preserve a0 in k1
	li	t9, 15		# UHI exception operation
	li	a0, 0		# Use hard register context
	sdbbp	1		# Invoke UHI operation
	.endm

	.macro setup_stack_gd
	li	t0, -16
	PTR_LI	t1, CONFIG_SYS_INIT_SP_ADDR
	and	sp, t1, t0		# force 16 byte alignment
	PTR_SUBU \
		sp, sp, GD_SIZE		# reserve space for gd
	and	sp, sp, t0		# force 16 byte alignment
	move	k0, sp			# save gd pointer
	move	fp, sp

	PTR_SUBU \
		sp, sp, 0x20		# stack gap, reserved spaces for o32 ABI args

	/* Clear gd */
	move	t0, k0
1:
	PTR_S	zero, 0(t0)
	blt	t0, t1, 1b
	 PTR_ADDIU t0, PTRSIZE
	.endm

ENTRY(_start)
	/* U-Boot SPL entry point */
	b	reset
	 mtc0	zero, CP0_COUNT	# clear cp0 count for most accurate boot timing

	# No more instructions here. Only two instructions allowed here as the
	# following spaces will be used by NAND-Booting mechanism and will be
	# filled with values used by BootROM

	/* Reserved configuration spaces */
	.org 0x40

	.globl	__rom_cfg
__rom_cfg:
	.word	MTK_ROM_CFG_MAGIC_MT7621		/* Magic "7621" */
	.word	__ld_rom_size				/* SPL size */
	.word	CONFIG_MT7621_SPL_FLASH_SECTOR_SIZE	/* Size alignment (flash sector size) */
	.word	0					/* Relocation info size, filled by mtk-mmap-pad */

#if defined(CONFIG_ROM_EXCEPTION_VECTORS)
	/*
	 * Exception vector entry points. When running from ROM, an exception
	 * cannot be handled. Halt execution and transfer control to debugger,
	 * if one is attached.
	 */
	.org 0x200
	/* TLB refill, 32 bit task */
	uhi_mips_exception

	.org 0x280
	/* XTLB refill, 64 bit task */
	uhi_mips_exception

	.org 0x300
	/* Cache error exception */
	uhi_mips_exception

	.org 0x380
	/* General exception */
	uhi_mips_exception

	.org 0x400
	/* Catch interrupt exceptions */
	uhi_mips_exception

	.org 0x480
	/* EJTAG debug exception */
1:	b	1b
	 nop

	.org 0x500
#endif

reset:
	/* Init CP0 Status */
	mfc0	t0, CP0_STATUS
	and	t0, ST0_IMPL
	or	t0, ST0_BEV | ST0_ERL
	mtc0	t0, CP0_STATUS

	/*
	 * Check whether CP0 Config1 is implemented. If not continue
	 * with legacy Watch register initialization.
	 */
	mfc0	t0, CP0_CONFIG
	bgez	t0, wr_legacy
	 nop

	/*
	 * Check WR bit in CP0 Config1 to determine if Watch registers
	 * are implemented.
	 */
	mfc0	t0, CP0_CONFIG, 1
	andi	t0, (1 << 3)
	beqz	t0, wr_done
	 nop

	/* Clear Watch Status bits and disable watch exceptions */
	li	t1, 0x7		# Clear I, R and W conditions
	init_wr	0
	init_wr	1
	init_wr	2
	init_wr	3
	b	wr_done
	 nop

wr_legacy:
	MTC0	zero, CP0_WATCHLO
	mtc0	zero, CP0_WATCHHI

wr_done:
	/* Clear WP, IV and SW interrupts */
	mtc0	zero, CP0_CAUSE

	/* Clear timer interrupt (CP0_COUNT cleared on branch to 'reset') */
	mtc0	zero, CP0_COMPARE

	/* Non-VPE0 goes to AMON wait code directly */
	mfc0	t0, CP0_TCBIND
	andi	t0, TCBIND_CURVPE
	bnez	t0, amon_vpe_entry
	 nop

	/* Non-Core0 */
	PTR_LI	t0, CKSEG1ADDR(CONFIG_MIPS_CM_BASE)
	lw	t1, GCR_CL_ID(t0)
	bnez	t1, amon_core_entry
	 nop

#ifndef CONFIG_MT7621_NAND_BOOT
	/*
	 * For booting from SPI, set KSEG0 Uncached.
	 * Otherwise we're running on locked L2 cache and do not touch the cache.
	 */
	mfc0	t0, CP0_CONFIG
	and	t0, t0, MIPS_CONF_IMPL
	or	t0, t0, CONF_CM_UNCACHED
	mtc0	t0, CP0_CONFIG
	ehb
#endif

#ifdef CONFIG_MIPS_CM
	bal	mips_cm_map
	 nop
#endif

	/* Configure the SRAM before it can be used */
	bal	mips_stack_sram_init
	 nop

	/* Set up initial stack and global data */
	setup_stack_gd

	/* Earliest point to set up debug uart */
	bal	debug_uart_init
	 nop

	/* Initialize any external memory */
	bal	lowlevel_init
	 nop

#ifdef CONFIG_MT7621_NAND_BOOT
	/*
	 * Currently we're running on locked L2 cache (on KSEG0).
	 * To reset the entire cache, we have to move ourself to uncached
	 * SDRAM (on KSEG1 with the same physical address) and then call
	 * mips_cache_reset in KSEG1.
	 * After mips_cache_reset finishes, it will jump back to KSEG0.
	 * As the entire cache has been invalidated, the address range we're
	 * running on KSEG0 is refilled with data we have copied to KSEG1
	 * previously.
	 */
	la	a0, __text_start		# source address (KSEG0)
	la	a1, __image_copy_end		# end address (KSEG0)
	la	t0, __rom_cfg			# struct rom_cfg
	lw	t1, 12(t0)			# fetch reloc data size
	addu	a1, t1				# calculate end address with reloc data (KSEG0)
	move	a2, a0
	li	t0, 5
	ins	a2, t0, 29, 3			# convert source address to KSEG1 as the dest address

1:	lw	t0, 0(a0)
	sw	t0, 0(a2)
	addiu	a0, 4
	addiu	a2, 4
	bne	a0, a1, 1b
	 nop

	/* Calculate KSEG1 address of mips_cache_reset */
	PTR_LA	t9, mips_cache_reset
	li	t0, 5
	ins	t9, t0, 29, 3
	jalr	t9				# return address is still in KSEG0
	 nop
#else
	/* Initialize caches... */
	bal	mips_cache_reset
	 nop
#endif

	/* Bootup secondary CPU */
	bal	cpu_secondary_init
	 nop

	move	a0, zero
	bal	board_init_f
	 move	ra, zero

	END(_start)
