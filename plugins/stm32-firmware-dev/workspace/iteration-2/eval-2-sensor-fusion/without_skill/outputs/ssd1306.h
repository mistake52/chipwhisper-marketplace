#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stddef.h>

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64
#define SSD1306_PAGES   (SSD1306_HEIGHT / 8) /* 8 pages */

/* Framebuffer: 128 columns x 8 pages (rows/8) = 1024 bytes */
extern uint8_t ssd1306_fb[SSD1306_WIDTH * SSD1306_PAGES];

/* Initialise the OLED (sends full init command sequence via SPI) */
void ssd1306_init(void);

/* Send the full framebuffer to the OLED */
void ssd1306_update(void);

/* Clear the framebuffer */
void ssd1306_clear(void);

/* Fill the framebuffer (test pattern) */
void ssd1306_fill(void);

/* Set a pixel in the framebuffer */
void ssd1306_set_pixel(uint8_t x, uint8_t y, uint8_t color);

/* Draw a filled rectangle */
void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);

/* Draw a single character at (x, y) in page coordinates (y = page 0-7) */
void ssd1306_draw_char(uint8_t x, uint8_t page, char c);

/* Draw a string */
void ssd1306_draw_string(uint8_t x, uint8_t page, const char *str);

/* Draw a line using Bresenham algorithm */
void ssd1306_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);

/* Draw a circle outline */
void ssd1306_draw_circle(uint8_t cx, uint8_t cy, uint8_t r);

/* Draw a filled circle */
void ssd1306_fill_circle(uint8_t cx, uint8_t cy, uint8_t r);

/* Draw a compass rose / heading indicator */
void ssd1306_draw_compass(uint8_t cx, uint8_t cy, uint8_t radius, float heading_deg);

/* Display floating point number at position */
void ssd1306_draw_float(uint8_t x, uint8_t page, float val, uint8_t decimals);

#endif /* SSD1306_H */
