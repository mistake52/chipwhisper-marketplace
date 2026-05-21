#ifndef SPI_DRIVER_H
#define SPI_DRIVER_H

#include "stm32f103.h"

/* SPI1 pins:
 * PA5 = SCK
 * PA7 = MOSI (SDA on OLED)
 * PB0 = RES (OLED reset) - GPIO output
 * PA4 = DC  (OLED data/command) - GPIO output
 */
#define OLED_RES_PORT  GPIOB
#define OLED_RES_PIN   0

#define OLED_DC_PORT   GPIOA
#define OLED_DC_PIN    4

void spi1_init(void);
void spi1_cs_low(void);
void spi1_cs_high(void);
void spi1_dc_command(void);
void spi1_dc_data(void);
void spi1_res_low(void);
void spi1_res_high(void);
void spi1_send_byte(uint8_t data);
void spi1_send_bytes(const uint8_t *data, uint32_t len);
uint8_t spi1_transfer(uint8_t data);

#endif /* SPI_DRIVER_H */
