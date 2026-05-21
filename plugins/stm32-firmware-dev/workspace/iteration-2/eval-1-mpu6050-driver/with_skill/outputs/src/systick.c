/**
 * systick.c — SysTick: 1ms interrupts from 72MHz SYSCLK
 *
 * Uses the Cortex-M3 SysTick timer (ARM core, address 0xE000E010).
 * SYSTICK_HZ = 1000 means reload = 72000000/1000 - 1 = 71999.
 */

#include "systick.h"
#include "main.h"

#define SYSTICK_BASE            0xE000E010UL
#define SYSTICK_CTRL            (*(volatile uint32_t*)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD            (*(volatile uint32_t*)(SYSTICK_BASE + 0x04))
#define SYSTICK_VAL             (*(volatile uint32_t*)(SYSTICK_BASE + 0x08))
#define SYSTICK_CALIB           (*(volatile uint32_t*)(SYSTICK_BASE + 0x0C))

#define SYSTICK_CTRL_ENABLE     (1UL << 0)
#define SYSTICK_CTRL_TICKINT    (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE  (1UL << 2)
#define SYSTICK_CTRL_COUNTFLAG  (1UL << 16)

static volatile uint32_t _millis = 0;

void systick_init(void)
{
    /* Reload value for 1ms: SYSCLK / 1000 - 1 */
    uint32_t reload = (SYSCLK_FREQ / SYSTICK_HZ) - 1;

    SYSTICK_LOAD = reload;
    SYSTICK_VAL  = 0;
    SYSTICK_CTRL = SYSTICK_CTRL_ENABLE
                 | SYSTICK_CTRL_TICKINT
                 | SYSTICK_CTRL_CLKSOURCE;  /* AHB clock (72MHz) */
}

uint32_t millis(void)
{
    return _millis;
}

void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms) {
        /* Spin */
    }
}

/* SysTick interrupt handler — called every 1ms */
void SysTick_Handler(void)
{
    _millis++;
}
