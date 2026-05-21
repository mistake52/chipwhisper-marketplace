/**
 * systick.c — SysTick timer for STM32F407 (Cortex-M4)
 *
 * Register values from general knowledge — NOT MCP-verified.
 * SysTick uses processor clock (HCLK = 168MHz) with 1ms tick.
 */

#include "main.h"

/* SysTick registers (Cortex-M4 private) */
#define SYSTICK_CTRL    (*(volatile uint32_t*)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD    (*(volatile uint32_t*)(SYSTICK_BASE + 0x04))
#define SYSTICK_VAL     (*(volatile uint32_t*)(SYSTICK_BASE + 0x08))

#define SYSTICK_CTRL_ENABLE     (1U << 0)
#define SYSTICK_CTRL_TICKINT    (1U << 1)
#define SYSTICK_CTRL_CLKSOURCE  (1U << 2)  /* 1 = processor clock (HCLK) */
#define SYSTICK_CTRL_COUNTFLAG  (1U << 16)

/* 1ms tick: 168MHz / 1000 - 1 = 167999 */
#define SYSTICK_LOAD_VALUE  ((HCLK_FREQ / 1000UL) - 1UL)

static volatile uint32_t _ticks = 0;

void SysTick_Handler(void)
{
    _ticks++;
}

void systick_init(void)
{
    _ticks = 0;
    SYSTICK_LOAD = SYSTICK_LOAD_VALUE;
    SYSTICK_VAL  = 0;
    SYSTICK_CTRL = SYSTICK_CTRL_ENABLE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_CLKSOURCE;
}

void delay_ms(uint32_t ms)
{
    uint32_t start = _ticks;
    while ((_ticks - start) < ms);
}

uint32_t millis(void)
{
    return _ticks;
}
