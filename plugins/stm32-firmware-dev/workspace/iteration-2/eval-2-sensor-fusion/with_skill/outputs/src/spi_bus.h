/**
 * spi_bus.h — SPI1 bus driver for STM32F103 (OLED SSD1306)
 *
 * MCP-Verified (RM0008 Rev 21, Section 25):
 *   SPI1 base = 0x40013000 (APB2 bus)
 *   Pins: PA5=SCK, PA7=MOSI (default, AF push-pull)
 *
 * Configuration:
 *   Master mode, 8-bit, CPOL=0 CPHA=0, f_PCLK/8 = 9MHz
 *   Software slave management (SSM=1, SSI=1) — no CS line needed for single device
 */

#ifndef SPI_BUS_H
#define SPI_BUS_H

#include <stdint.h>

void spi_init(void);
void spi_write(uint8_t data);
uint8_t spi_transfer(uint8_t data);

#endif /* SPI_BUS_H */
