/**
 * systick.h — SysTick timer for 1ms tick
 *
 * Cortex-M3 SysTick is at 0xE000E010 (ARM core peripheral, not STM32-specific).
 */

#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

void systick_init(void);
uint32_t millis(void);
void delay_ms(uint32_t ms);

#endif /* SYSTICK_H */
