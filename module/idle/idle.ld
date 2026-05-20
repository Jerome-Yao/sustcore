ENTRY(_start)
OUTPUT_ARCH(riscv)

BASE_ADDRESS = 0x80000000;

SECTIONS
{
	. = BASE_ADDRESS;

	. = ALIGN(4K);
	.text : {
		*(.text.entry)
		*(.text .text.*)
	}

	. = ALIGN(4K);
	_end = .;

	/DISCARD/ : {
		*(.comment .comment.*)
		*(.note .note.*)
	}
}
