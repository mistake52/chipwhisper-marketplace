/**
 * ssd1306.h — SSD1306 128x64 OLED display driver (SPI mode)
 *
 * 6-pin module: GND, VCC, SCK, SDA(MOSI), RES, DC
 * CS is internally tied to GND.
 *
 * SPI pins: PA5=SCK, PA7=MOSI
 * GPIO:    PB0=RES (active low), PB1=DC (data/command)
 */

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

void ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_update(void);
void ssd1306_draw_pixel(uint8_t x, uint8_t y, uint8_t color);
void ssd1306_draw_char(uint8_t x, uint8_t y, char c, uint8_t invert);
void ssd1306_draw_string(uint8_t x, uint8_t y, const char *str, uint8_t invert);
void ssd1306_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void ssd1306_set_contrast(uint8_t contrast);

#endif /* SSD1306_H */
