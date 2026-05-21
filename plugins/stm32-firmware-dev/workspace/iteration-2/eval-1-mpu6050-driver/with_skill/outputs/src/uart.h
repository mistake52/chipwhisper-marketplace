/**
 * uart.h — USART1 serial output, MCP-verified register layout
 *
 * MCP-verified (RM0008 Rev 21, sec 27.6):
 *   USART1:       0x40013800 (APB2)
 *   Pin:          PA9 = TX (AF_PP)
 *   Clock enable: RCC_APB2ENR bit 14 (USART1EN)
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_print_int(int32_t val);
void uart_print_float(float val, int32_t decimals);

#endif /* UART_H */
