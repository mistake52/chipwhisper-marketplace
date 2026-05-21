/**
 * lm75.h — LM75 I2C temperature sensor driver header
 * Register values from general knowledge — NOT MCP-verified.
 */

#ifndef LM75_H
#define LM75_H

#include <stdint.h>

int  lm75_read_temp(int16_t *temp_raw);
void lm75_to_celsius(int16_t raw, int *whole, int *tenths);

#endif /* LM75_H */
