#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include "stm32f103.h"

/* I2C1 pins: PB6=SCL, PB7=SDA */
#define I2C1_SCL_PIN  6
#define I2C1_SDA_PIN  7

/* Initialise I2C1 at 100kHz standard mode (PCLK1=36MHz) */
void i2c1_init(void);

/* Write one byte to I2C slave (no sub-address) */
int i2c1_write_byte(uint8_t dev_addr, uint8_t data);

/* Write to I2C register (1-byte register address) */
int i2c1_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data);

/* Read from I2C register (1-byte register address) */
int i2c1_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data);

/* Multi-byte read from consecutive I2C registers */
int i2c1_read_regs(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint8_t len);

/* Probe an I2C address (returns 0 if ACK received) */
int i2c1_probe(uint8_t dev_addr);

/* Return the last SR1 value (for diagnostics) */
uint32_t i2c1_get_last_sr1(void);

#endif /* I2C_DRIVER_H */
