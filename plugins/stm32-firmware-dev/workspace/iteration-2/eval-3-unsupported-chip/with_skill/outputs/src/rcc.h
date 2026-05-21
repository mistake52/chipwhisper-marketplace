/**
 * rcc.h — Clock configuration header
 * Register values from general knowledge — NOT MCP-verified.
 */

#ifndef RCC_H
#define RCC_H

void rcc_init(void);
void rcc_enable_gpio(void);
void rcc_enable_i2c1(void);
void rcc_enable_spi1(void);

#endif /* RCC_H */
