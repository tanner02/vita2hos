OUTPUT_FORMAT(elf32-littlearm)
ENTRY(module_info)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
    . = 0x81000000;
    .text : { *(.text*) *(.rodata*) } :text
    . = 0x81879000;
    .data : { *(.data*) } :data
    .bss (NOLOAD) : { *(.bss*) } :data
}
