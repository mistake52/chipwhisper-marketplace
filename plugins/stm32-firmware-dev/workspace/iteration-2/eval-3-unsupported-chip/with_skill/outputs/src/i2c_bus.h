/**
 * i2c_bus.h — I2C1 bus driver header
 * Register values from general knowledge — NOT MCP-verified.
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>

void i2c_init(void);
int  i2c_detect(uint8_t addr);
int  i2c_write_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, int len);

#endif /* I2C_BUS_H */
