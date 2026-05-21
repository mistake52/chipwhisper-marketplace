/**
 * i2c_bus.h — I2C1 bus driver, master mode, 100kHz standard
 *
 * MCP-verified register layout (RM0008 Rev 21, sec 26.6):
 *   I2C1 base: 0x40005400 (APB1)
 *   Pins: PB6=SCL(AF_OD), PB7=SDA(AF_OD)
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>

void i2c_init(void);
void i2c_start(void);
void i2c_stop(void);
void i2c_write_addr(uint8_t addr, uint8_t dir);  /* dir: 0=write, 1=read */
void i2c_write_byte(uint8_t data);
uint8_t i2c_read_byte(uint8_t ack);  /* ack: 0=NAK, 1=ACK */
uint8_t i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr);
void i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
void i2c_read_multi(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len);

#endif /* I2C_BUS_H */
