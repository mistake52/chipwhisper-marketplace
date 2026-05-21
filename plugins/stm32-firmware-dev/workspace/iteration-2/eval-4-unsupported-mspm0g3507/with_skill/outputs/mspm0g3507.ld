/**
 * mspm0g3507.ld — Linker script for MSPM0G3507
 *
 * Memory layout (from SLASEX6 datasheet):
 *   Flash: 128 KB @ 0x0000_0000
 *   SRAM:   32 KB @ 0x2000_0000
 *
 * MANUAL ONLY: Memory sizes from TI datasheet, not MCP-verified.
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 128K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH =  32K
}

__stack_size = 0x0400;   /* 1 KB stack */

ENTRY(Reset_Handler)

SECTIONS
{
    /* Vector table MUST be at beginning of flash */
    .vectors :
    {
        KEEP(*(.vectors))
    } > FLASH

    /* Program code */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
    } > FLASH

    /* Initialized data — stored in flash, copied to SRAM at startup */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT > FLASH

    /* Uninitialized data — zeroed at startup */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > SRAM

    /* Stack — top of SRAM, grows downward (full-descending) */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + __stack_size;
        . = ALIGN(8);
        __stack_top = .;
    } > SRAM
}
