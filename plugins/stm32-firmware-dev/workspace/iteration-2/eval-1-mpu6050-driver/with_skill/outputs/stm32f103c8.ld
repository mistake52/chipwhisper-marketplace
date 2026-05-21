/**
 * stm32f103c8.ld — Linker script for STM32F103C8T6
 *
 * Flash: 64KB (0x08000000 - 0x0800FFFF)
 * RAM:   20KB (0x20000000 - 0x20004FFF)
 *
 * Stack (top of RAM): 0x20005000
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_stack_end = ORIGIN(RAM) + LENGTH(RAM);   /* 0x20005000 */
_stack_size = 0x400;                       /* 1KB stack */

SECTIONS
{
    /* Vector table and code in flash */
    .text :
    {
        KEEP(*(.isr_vector))
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* Initialized data: copied from flash to RAM at startup */
    _sidata = LOADADDR(.data);
    .data : AT (_etext)
    {
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;
    } > RAM

    /* Uninitialized data (BSS): zeroed at startup */
    .bss :
    {
        _sbss = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* Stack at top of RAM */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        _stack_start = .;
        . = . + _stack_size;
        . = ALIGN(8);
    } > RAM
}
