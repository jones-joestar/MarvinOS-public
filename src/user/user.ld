ENTRY(_start)

SECTIONS {
    . = 0x10000;

    .text : {
        *(.text.entry)
        *(.text*)
    }

    . = ALIGN(4096);
    .rodata : {
        *(.rodata*)
    }

    . = ALIGN(4096);
    .data : {
        *(.data*)
    }

    . = ALIGN(4096);
    .bss : {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }
}
