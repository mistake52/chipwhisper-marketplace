/**
 * @file    stm32f103c8t6.ld
 * @brief   Linker script for STM32F103C8T6 (64 KB Flash, 20 KB RAM).
 */

ENTRY(Reset_Handler)

/* ----------------------------------------------------------------------- */
/* Memory layout                                                           */
/* ----------------------------------------------------------------------- */
MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
    RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* Top of stack */

/* ----------------------------------------------------------------------- */
/* Sections                                                                */
/* ----------------------------------------------------------------------- */
SECTIONS
{
    /* ---- Vector table must be at the start of FLASH ---- */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* ---- Code ---- */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* ---- Used by Reset_Handler to copy .data from FLASH ---- */
    _sidata = LOADADDR(.data);

    /* ---- Initialised data in RAM (copied from FLASH at startup) ---- */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    /* ---- Zero-initialised data ---- */
    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* ---- Discard everything else ---- */
    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.comment)
        *(.note*)
    }
}
