#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart1_init(void);
void uart1_putc(char c);
void uart1_puts(const char *s);
void uart1_print_reg(const char *name, uint32_t val, uint32_t expected_mask,
                     uint32_t expected_val);

#endif /* UART_H */
