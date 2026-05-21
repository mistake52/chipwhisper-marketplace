/**
 * @file    mspm0g3507.ld
 * @brief   Linker script for MSPM0G3507 (128 KB Flash, 32 KB SRAM)
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 128K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH =  32K
}

/* End of stack: top of SRAM */
_estack = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    /* ------------------------------------------------------------------ */
    /* .isr_vector: placed at the start of Flash (0x00000000)              */
    /* ------------------------------------------------------------------ */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))     /* Vector table from startup code */
        . = ALIGN(4);
    } > FLASH

    /* ------------------------------------------------------------------ */
    /* .text: program code                                                 */
    /* ------------------------------------------------------------------ */
    .text :
    {
        . = ALIGN(4);
        *(.text)                 /* .text sections                       */
        *(.text.*)               /* .text.func sections (from -ffunction-sections) */
        *(.rodata)               /* .rodata sections                     */
        *(.rodata.*)             /* .rodata.const sections               */
        *(.glue_7)               /* ARM/Thumb interworking glue          */
        *(.glue_7t)
        . = ALIGN(4);
        _etext = .;              /* End of text in Flash                 */
    } > FLASH

    /* ------------------------------------------------------------------ */
    /* .ARM.extab / .ARM.exidx: exception unwinding tables                */
    /* ------------------------------------------------------------------ */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    /* ------------------------------------------------------------------ */
    /* .preinit_array / .init_array / .fini_array: C++ static ctors/dtors */
    /* ------------------------------------------------------------------ */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    /* ------------------------------------------------------------------ */
    /* _sidata: start of .data initial values in Flash                    */
    /* This symbol is used by the startup code to copy .data to SRAM.     */
    /* ------------------------------------------------------------------ */
    _sidata = LOADADDR(.data);

    /* ------------------------------------------------------------------ */
    /* .data: initialized global/static variables                         */
    /* VMA (SRAM) for runtime, LMA (Flash) for initial values.            */
    /* ------------------------------------------------------------------ */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;              /* Start of .data in SRAM               */
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;              /* End   of .data in SRAM               */
    } > SRAM AT > FLASH

    /* ------------------------------------------------------------------ */
    /* .bss: zero-initialized global/static variables                     */
    /* ------------------------------------------------------------------ */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;               /* Start of .bss in SRAM                */
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;               /* End   of .bss in SRAM                */
    } > SRAM

    /* ------------------------------------------------------------------ */
    /* .heap: dynamically allocated memory (optional)                     */
    /* ------------------------------------------------------------------ */
    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __heap_start__ = .;
        . = . + 0x400;           /* 1 KB heap (adjust as needed)         */
        __heap_end__ = .;
    } > SRAM

    /* ------------------------------------------------------------------ */
    /* Discard sections not needed in embedded context                    */
    /* ------------------------------------------------------------------ */
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
        *(.comment)
        *(.note)
        *(.eh_frame)
    }
}
