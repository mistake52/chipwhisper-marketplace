/* Linker script for STM32F103C8T6 / QEMU stm32vldiscovery
 *
 * STM32F103C8T6: 64KB Flash, 20KB RAM
 * stm32vldiscovery (QEMU): STM32F100 with 128KB Flash, 8KB RAM
 *
 * For QEMU testing we target 8KB RAM to match the stm32vldiscovery machine.
 * For real STM32F103C8 hardware, change RAM LENGTH to 20K.
 */

/* Uncomment for hardware target: */
/* RAM_LENGTH = 20K; */
/* For QEMU stm32vldiscovery: */
RAM_LENGTH = 8K;

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 128K
    RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = RAM_LENGTH
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector : {
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text : {
        KEEP(*(.init))
        *(.text*)
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    .rodata : {
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data : AT (_etext) {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM

    .bss : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
}
