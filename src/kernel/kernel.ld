ENTRY(_start)

SECTIONS {
    /* Compile for higher half mapping + 2MB offset */
    . = 0xFFFFFFFF80000000 + 0x200000;

    __kernel_start = .;

    __text_start = .;
    /* AT directive specifies where the booloader places the section */
    .text : AT(ADDR(.text) - 0xFFFFFFFF80000000) {
        *(.text.entry)
        *(.text*)
    }
    __text_end = .;

    /* sections are aligned so that we can map them with 4kb pages and correct flags */
    . = ALIGN(4096);
    __rodata_start = .;
    .rodata : AT(ADDR(.rodata) - 0xFFFFFFFF80000000) {
        *(.rodata*)
    }
    __rodata_end = .;

    . = ALIGN(4096);
    __data_start = .;
    .data : AT(ADDR(.data) - 0xFFFFFFFF80000000) {
        *(.data*)
    }
    __data_end = .;

    . = ALIGN(4096);
    __bss_start = .;
    .bss : AT(ADDR(.bss) - 0xFFFFFFFF80000000) {
        *(COMMON)
        *(.bss*)
    }
    __bss_end = .;

    . = ALIGN(4096);
    __kernel_end = .;
}
