/**
 * lcd_ili9341.h — ILI9341 TFT LCD driver header
 * Register values from general knowledge — NOT MCP-verified.
 */

#ifndef LCD_ILI9341_H
#define LCD_ILI9341_H

#include <stdint.h>

void lcd_init(void);
void lcd_clear(uint16_t color);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg);
void lcd_draw_string(int x, int y, const char *str, uint16_t fg, uint16_t bg);
void lcd_show_temperature(int whole, int tenths);

#endif /* LCD_ILI9341_H */
