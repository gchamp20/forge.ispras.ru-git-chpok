OUTPUT_FORMAT("elf32-i386");
OUTPUT_ARCH("i386")
ENTRY(__pok_partition_start)

SECTIONS
{
	kshd = 0; /* This should coincide with ja_space_shared_data() in the kernel. */

	. = 0x1000;

	__partition_begin = . ;

	.text :
	{
		*(.text)
	}

	.rodata :
	{
		*(.rodata)
	}

	.data :
	{
		*(.data) *(.bss) *(COMMON)
	}

	__partition_end = . ;
}
