/**
 * systick.c — SysTick timer configuration
 *
 * SysTick is a Cortex-M3 core peripheral at 0xE000E010.
 * Configured for 1ms interrupt using SYSCLK as clock source.
 */

#include "systick.h"
#include "main.h"

/* SysTick register definitions (Cortex-M3 core peripheral) */
#define SYSTICK_BASE    0xE000E010UL
#define SYSTICK_CSR     (*(volatile uint32_t*)(SYSTICK_BASE + 0x00))
#define SYSTICK_RVR     (*(volatile uint32_t*)(SYSTICK_BASE + 0x04))
#define SYSTICK_CVR     (*(volatile uint32_t*)(SYSTICK_BASE + 0x08))

/* CSR bits */
#define SYSTICK_CSR_ENABLE      (1UL << 0)
#define SYSTICK_CSR_TICKINT     (1UL << 1)
#define SYSTICK_CSR_CLKSOURCE   (1UL << 2)
#define SYSTICK_CSR_COUNTFLAG   (1UL << 16)

static volatile uint32_t sys_tick_ms = 0;

/**
 * Initialize SysTick for 1ms intervals.
 *
 * For SYSCLK=72MHz: RELOAD = 72000000/1000 - 1 = 71999
 * For SYSCLK=8MHz:  RELOAD = 8000000/1000 - 1 = 7999
 */
void systick_init(void) {
    uint32_t reload = (SYSCLK_FREQ / 1000UL) - 1UL;
    SYSTICK_RVR = reload;         /* Set reload value */
    SYSTICK_CVR = 0;              /* Clear current value */
    SYSTICK_CSR = (SYSTICK_CSR_ENABLE    |
                   SYSTICK_CSR_TICKINT   |
                   SYSTICK_CSR_CLKSOURCE);
    sys_tick_ms = 0;
}

/**
 * SysTick interrupt handler.
 */
void SysTick_Handler(void) {
    sys_tick_ms++;
}

/**
 * Get elapsed milliseconds.
 */
uint32_t millis(void) {
    return sys_tick_ms;
}

/**
 * Blocking delay in milliseconds.
 */
void delay_ms(uint32_t ms) {
    uint32_t start = millis();
    while ((millis() - start) < ms) {
        __asm__ volatile ("nop");
    }
}
