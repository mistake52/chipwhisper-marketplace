/**
 * @file    lcd_ili9341.h
 * @brief   ILI9341 TFT LCD display driver header (SPI interface)
 *
 * ILI9341 is a 240x320 TFT LCD controller. This driver communicates
 * via a 4-wire SPI interface (SCK, MOSI, DC, CS, RST optional).
 *
 * Pin configuration (STM32F407VGT6 Discovery):
 *   SPI2_SCK  : PB13 (AF5) -- SPI2 clock
 *   SPI2_MOSI : PB15 (AF5) -- SPI2 data output
 *   LCD_CS    : PE0  (GPIO Output, software-controlled)
 *   LCD_DC    : PE1  (GPIO Output)  -- Data/Command select
 *   LCD_RST   : PE2  (GPIO Output)  -- Hardware reset
 */

#ifndef LCD_ILI9341_H
#define LCD_ILI9341_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * LCD Dimensions
 * -------------------------------------------------------------------------- */
#define ILI9341_WIDTH   240U
#define ILI9341_HEIGHT  320U

/* --------------------------------------------------------------------------
 * Color definitions (RGB565 format, 16-bit)
 * -------------------------------------------------------------------------- */
#define COLOR_BLACK       0x0000U
#define COLOR_WHITE       0xFFFFU
#define COLOR_RED         0xF800U
#define COLOR_GREEN       0x07E0U
#define COLOR_BLUE        0x001FU
#define COLOR_YELLOW      0xFFE0U
#define COLOR_CYAN        0x07FFU
#define COLOR_MAGENTA     0xF81FU
#define COLOR_GRAY        0x8410U
#define COLOR_DARK_GRAY   0x4208U
#define COLOR_ORANGE      0xFD20U
#define COLOR_LIGHT_BLUE  0x041FU
#define COLOR_NAVY        0x000FU
#define COLOR_DARK_GREEN  0x03E0U

/* Macro to create RGB565 color from 8-bit R, G, B components */
#define RGB565(r, g, b)   ((((uint16_t)(r) & 0xF8U) << 8) | \
                            (((uint16_t)(g) & 0xFCU) << 3) | \
                            (((uint16_t)(b) & 0xF8U) >> 3))

/* --------------------------------------------------------------------------
 * Font sizes
 * -------------------------------------------------------------------------- */
typedef enum {
    FONT_SMALL  = 0,  /* 6x8 font (5x7 character + spacing) */
    FONT_MEDIUM = 1,  /* 12x16 (doubled) */
    FONT_LARGE  = 2   /* 24x32 (quadrupled) */
} FontSize;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief  Initialize the ILI9341 LCD controller.
 *         Configures SPI, resets the display, sends init sequence.
 * @param  hspi Handle of the SPI peripheral (SPI_HandleTypeDef*)
 * @return true on success, false on failure
 */
bool LCD_Init(void *hspi);

/**
 * @brief  Fill the entire screen with a single color.
 * @param  color 16-bit RGB565 color value
 */
void LCD_FillScreen(uint16_t color);

/**
 * @brief  Fill a rectangular area with a single color.
 * @param  x     Start X coordinate (0 = left)
 * @param  y     Start Y coordinate (0 = top)
 * @param  w     Width in pixels
 * @param  h     Height in pixels
 * @param  color 16-bit RGB565 color value
 */
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief  Set the address window for subsequent pixel writes.
 * @param  x0 Start X
 * @param  y0 Start Y
 * @param  x1 End X
 * @param  y1 End Y
 */
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief  Draw a single pixel.
 * @param  x     X coordinate
 * @param  y     Y coordinate
 * @param  color 16-bit RGB565 color
 */
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief  Draw a single character at the specified position.
 *         Uses a built-in 5x7 monospace font.
 * @param  x     X coordinate (left)
 * @param  y     Y coordinate (top)
 * @param  ch    ASCII character to draw
 * @param  color Foreground color
 * @param  bg    Background color
 * @param  size  Font size multiplier
 */
void LCD_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color,
                  uint16_t bg, FontSize size);

/**
 * @brief  Draw a null-terminated string at the specified position.
 * @param  x     X coordinate
 * @param  y     Y coordinate
 * @param  str   Null-terminated ASCII string
 * @param  color Foreground color
 * @param  bg    Background color
 * @param  size  Font size multiplier
 */
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color,
                    uint16_t bg, FontSize size);

/**
 * @brief  Draw a horizontal line.
 * @param  x     Start X
 * @param  y     Y coordinate
 * @param  len   Length in pixels
 * @param  color 16-bit RGB565 color
 */
void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color);

/**
 * @brief  Draw a vertical line.
 * @param  x     X coordinate
 * @param  y     Start Y
 * @param  len   Length in pixels
 * @param  color 16-bit RGB565 color
 */
void LCD_DrawVLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color);

/**
 * @brief  Get the character width for the specified font size.
 * @param  size Font size
 * @return Character width in pixels
 */
uint16_t LCD_CharWidth(FontSize size);

/**
 * @brief  Get the character height for the specified font size.
 * @param  size Font size
 * @return Character height in pixels
 */
uint16_t LCD_CharHeight(FontSize size);

/**
 * @brief  Set rotation of the display.
 * @param  rotation 0=portrait(240x320), 1=landscape(320x240),
 *                  2=portrait inverted, 3=landscape inverted
 */
void LCD_SetRotation(uint8_t rotation);

#ifdef __cplusplus
}
#endif

#endif /* LCD_ILI9341_H */
