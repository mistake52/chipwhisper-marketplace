/**
 * systick.h — SysTick timer header
 * Register values from general knowledge — NOT MCP-verified.
 */

#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

void systick_init(void);
void delay_ms(uint32_t ms);
uint32_t millis(void);

void SysTick_Handler(void);

#endif /* SYSTICK_H */
