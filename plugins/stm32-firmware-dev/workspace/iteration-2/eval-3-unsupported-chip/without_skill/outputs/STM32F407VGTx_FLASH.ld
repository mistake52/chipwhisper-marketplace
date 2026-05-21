/**
 * @file    STM32F407VGTx_FLASH.ld
 * @brief   GNU linker script for STM32F407VGT6 (1MB Flash, 192KB RAM + 64KB CCM)
 *
 * Memory layout:
 *   Flash:  0x08000000 - 0x080FFFFF (1 MB)
 *   RAM:    0x20000000 - 0x2002FFFF (192 KB)
 *   CCMRAM: 0x10000000 - 0x1000FFFF (64 KB, tightly coupled, no DMA)
 *
 * Stack starts at the end of regular SRAM; heap occupies the remainder.
 */

/* --------------------------------------------------------------------------
 * Memory Definitions
 * -------------------------------------------------------------------------- */
MEMORY
{
    FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    RAM     (rwx) : ORIGIN = 0x20000000, LENGTH = 192K
    CCMRAM  (rwx) : ORIGIN = 0x10000000, LENGTH = 64K
}

/* --------------------------------------------------------------------------
 * Stack and Heap sizes
 * -------------------------------------------------------------------------- */
_estack     = ORIGIN(RAM) + LENGTH(RAM);   /* Top of stack (grows downward) */
_Min_Heap_Size  = 0x000;    /* 0 bytes heap (use CCMRAM heap if needed) */
_Min_Stack_Size = 0x800;    /* 2 KB minimum stack */

/* --------------------------------------------------------------------------
 * Output Sections
 * -------------------------------------------------------------------------- */
SECTIONS
{
    /* ---- Interrupt Vector Table (must be at Flash start) ---- */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* ---- Program code ---- */
    .text :
    {
        . = ALIGN(4);
        *(.text)           /* .text sections (code) */
        *(.text*)          /* .text.* sections (code) */
        *(.glue_7)         /* ARM/Thumb interworking glue */
        *(.glue_7t)
        *(.eh_frame)       /* Exception handling frame */

        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;        /* End of code */
    } > FLASH

    /* ---- Read-only data ---- */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* ---- ARM exception unwind information ---- */
    .ARM.extab :
    {
        . = ALIGN(4);
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } > FLASH

    .ARM.exidx :
    {
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
        . = ALIGN(4);
    } > FLASH

    /* ---- Preinit/init/fini arrays ---- */
    .preinit_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
        . = ALIGN(4);
    } > FLASH

    .init_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
        . = ALIGN(4);
    } > FLASH

    .fini_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
        . = ALIGN(4);
    } > FLASH

    /* ---- Flash-end marker for data initialization ---- */
    _sidata = LOADADDR(.data);

    /* ---- Initialized data (copied from Flash to RAM at startup) ---- */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;         /* Start of data in RAM */
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;         /* End of data in RAM */
    } > RAM AT > FLASH

    /* ---- Uninitialized data (BSS) ---- */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = .;
    } > RAM

    /* ---- Heap (placed after BSS in RAM) ---- */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > RAM

    /* ---- CCMRAM section (optional tightly-coupled memory) ---- */
    .ccmram :
    {
        . = ALIGN(4);
        *(.ccmram)
        *(.ccmram*)
        . = ALIGN(4);
    } > CCMRAM

    /* ---- Remove information from standard libraries ---- */
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }
}
