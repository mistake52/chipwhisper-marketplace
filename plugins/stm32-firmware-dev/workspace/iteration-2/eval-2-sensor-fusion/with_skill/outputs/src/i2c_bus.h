/**
 * i2c_bus.h — I2C1 bus driver for STM32F103
 *
 * MCP-Verified (RM0008 Rev 21, Section 26):
 *   I2C1 base = 0x40005400 (APB1 bus)
 *   Pins: PB6=SCL, PB7=SDA (default mapping, AF open-drain)
 *
 * Timing for 100kHz standard mode with APB1=36MHz:
 *   CR2.FREQ = 36 (0x24)
 *   CCR = FREQ × 10μs / 2 = 180 (0xB4)  [standard mode, DUTY=0]
 *   TRISE = FREQ + 1 = 37 (0x25)         [standard mode]
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>

void i2c_init(void);
void i2c_start(void);
void i2c_stop(void);
void i2c_write_addr(uint8_t addr, uint8_t rw);
void i2c_write_byte(uint8_t data);
uint8_t i2c_read_byte(uint8_t ack);
uint8_t i2c_read_nack(void);
int i2c_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data);
int i2c_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data);
int i2c_read_regs(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint8_t len);

#endif /* I2C_BUS_H */
