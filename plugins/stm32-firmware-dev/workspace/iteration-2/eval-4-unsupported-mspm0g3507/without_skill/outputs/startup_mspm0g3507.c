/**
 * @file    startup_mspm0g3507.c
 * @brief   Startup code for MSPM0G3507 (ARM Cortex-M0+)
 *
 * Sets up the vector table, initializes .data and .bss sections,
 * and calls main().
 *
 * MSPM0G3507 memory map:
 *   Flash: 128 KB @ 0x00000000
 *   SRAM:   32 KB @ 0x20000000
 */

#include <stdint.h>

/* --------------------------------------------------------------------------
 * Linker-defined symbols
 * -------------------------------------------------------------------------- */
extern uint32_t _sidata;    /* Start address of .data initial values in Flash */
extern uint32_t _sdata;     /* Start address of .data section in SRAM         */
extern uint32_t _edata;     /* End   address of .data section in SRAM         */
extern uint32_t _sbss;      /* Start address of .bss  section in SRAM         */
extern uint32_t _ebss;      /* End   address of .bss  section in SRAM         */
extern uint32_t _estack;    /* Top of stack (from linker script)              */

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
int  main(void);
void SystemInit(void);
void Default_Handler(void);

/* Externally-defined interrupt handlers (weak in main.c) */
void SysTick_Handler(void);

/* --------------------------------------------------------------------------
 * Minimal SystemInit - called before main()
 * -------------------------------------------------------------------------- */
void SystemInit(void)
{
    /*
     * On MSPM0G3507, after reset the device runs from the internal
     * 32 MHz oscillator (SYSOSC). No PLL or external crystal setup
     * is needed for this simple application.
     *
     * If a different clock configuration is required, it should be
     * done here before main().
     */
}

/* --------------------------------------------------------------------------
 * Reset Handler
 * -------------------------------------------------------------------------- */
__attribute__((naked, noreturn))
void Reset_Handler(void)
{
    uint32_t *src, *dst;

    /* 1. Set the main stack pointer from the vector table entry */
    __asm volatile (
        "ldr   r0, =_estack\n\t"
        "msr   msp, r0\n\t"
    );

    /* 2. Copy .data section from Flash to SRAM */
    src = &_sidata;
    dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* 3. Zero-initialize .bss section */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* 4. Call system initialization */
    SystemInit();

    /* 5. Call main() - should never return */
    main();

    /* Trap if main returns */
    while (1) { }
}

/* --------------------------------------------------------------------------
 * Default Handler - infinite loop for unexpected interrupts
 * -------------------------------------------------------------------------- */
void Default_Handler(void)
{
    while (1) { }
}

/* --------------------------------------------------------------------------
 * Vector Table for MSPM0G3507 (Cortex-M0+)
 *
 * The first entry is the initial stack pointer, the second is the
 * reset vector. Cortex-M0+ uses a subset of the full ARM vector table
 * (16 system exceptions + device-specific IRQs).
 *
 * MSPM0G3507 IRQs start at vector offset 16.
 * Only IRQs relevant to this application are populated; all others
 * fall through to Default_Handler.
 * -------------------------------------------------------------------------- */

/* Declare all handlers as weak aliases of Default_Handler */
#define WEAK_ALIAS(f) __attribute__((weak, alias("Default_Handler")))

void NMI_Handler(void)              WEAK_ALIAS;
void HardFault_Handler(void)        WEAK_ALIAS;
/* Vector 5 (MemManage) is reserved on Cortex-M0+ */
/* Vector 6 (BusFault)   is reserved on Cortex-M0+ */
/* Vector 7 (UsageFault) is reserved on Cortex-M0+ */
void SVC_Handler(void)              WEAK_ALIAS;
void DebugMon_Handler(void)         WEAK_ALIAS;
void PendSV_Handler(void)           WEAK_ALIAS;
void SysTick_Handler(void)          WEAK_ALIAS;

/* MSPM0G3507 peripheral IRQ handlers (partial list) */
void ADC0_IRQHandler(void)          WEAK_ALIAS;
void GPIOA_IRQHandler(void)         WEAK_ALIAS;

/*
 * Full vector table.
 * The __attribute__((used)) prevents the linker from discarding
 * this section when --gc-sections is active.
 */
__attribute__((section(".isr_vector"), used))
const void *vector_table[] = {
    /* 0x00: Initial Stack Pointer */
    (void *)&_estack,

    /* 0x04: Reset */
    (void *)Reset_Handler,

    /* 0x08: NMI */
    (void *)NMI_Handler,

    /* 0x0C: HardFault */
    (void *)HardFault_Handler,

    /* 0x10: Reserved (MemManage on M3/M4, not present on M0+) */
    0,

    /* 0x14: Reserved (BusFault on M3/M4, not present on M0+) */
    0,

    /* 0x18: Reserved (UsageFault on M3/M4, not present on M0+) */
    0,

    /* 0x1C - 0x28: Reserved */
    0, 0, 0, 0,

    /* 0x2C: SVCall */
    (void *)SVC_Handler,

    /* 0x30: Debug Monitor */
    (void *)DebugMon_Handler,

    /* 0x34: Reserved */
    0,

    /* 0x38: PendSV */
    (void *)PendSV_Handler,

    /* 0x3C: SysTick */
    (void *)SysTick_Handler,

    /* ================================================================
     * Peripheral IRQs (starting at index 16, offset 0x40)
     *
     * MSPM0G3507 has approximately 32-48 peripheral interrupts.
     * Below is a minimal but correct table that includes the IRQs
     * used by this application. Unused entries are NULL pointers
     * and will cause a HardFault if triggered unexpectedly.
     * ================================================================ */

    /* IRQ 0 (vector 16) - ADC0 */
    (void *)ADC0_IRQHandler,

    /* IRQ 1 (vector 17) - ADC1 */
    0,

    /* IRQ 2 (vector 18) - UART0 */
    0,

    /* IRQ 3 (vector 19) - UART1 */
    0,

    /* IRQ 4 (vector 20) - I2C0 */
    0,

    /* IRQ 5 (vector 21) - SPI0 */
    0,

    /* IRQ 6 (vector 22) - TIMG0 */
    0,

    /* IRQ 7 (vector 23) - TIMG1 */
    0,

    /* IRQ 8 (vector 24) - GPIOA */
    (void *)GPIOA_IRQHandler,

    /* IRQ 9 (vector 25) - GPIOB */
    0,

    /* IRQ 10-15: Reserved / other peripherals */
    0, 0, 0, 0, 0, 0,

    /* IRQ 16 (vector 32) - COMP0 */
    0,

    /* IRQ 17 (vector 33) - COMP1 */
    0,

    /*
     * For brevity, the remaining IRQ slots are zeroed.
     * In a production system, they should map to valid handlers
     * or Default_Handler to prevent undefined behavior.
     */
};
