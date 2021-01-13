/*
 * SPDX-License-Identifier:	GPL-2.0+
 */

OUTPUT_ARCH(mips)
ENTRY(_start)
SECTIONS
{
	. = 0x00000000;

	. = ALIGN(4);
	.text : {
		__text_start = .;
		*(.text*)
		__text_end = .;
	}

	. = ALIGN(4);
	.rodata : {
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*)))
	}

	. = ALIGN(4);
	.data : {
		*(SORT_BY_ALIGNMENT(.data*))
		*(SORT_BY_ALIGNMENT(.sdata*))
	}

	. = ALIGN(4);
	.u_boot_list : {
		KEEP(*(SORT(.u_boot_list*)));
	}

	. = ALIGN(4);
	__image_copy_end = .;
	__rel_start = . + 8;

	__ld_rom_size = __image_copy_end - __text_start;

	.bss : {
		__bss_start = .;
		*(.bss*)
		*(.sbss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end = .;
	}

	.rel.dyn : {
		*(.rel.dyn)
	}

	.dynsym : {
		*(.dynsym)
	}

	.dynbss : {
		*(.dynbss)
	}

	.dynstr : {
		*(.dynstr)
	}

	.dynamic : {
		*(.dynamic)
	}

	.plt : {
		*(.plt)
	}

	.interp : {
		*(.interp)
	}

	.gnu : {
		*(.gnu*)
	}

	.MIPS.stubs : {
		*(.MIPS.stubs)
	}

	.hash : {
		*(.hash)
	}
}
