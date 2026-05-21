/**
 * @file    lcd_ili9341.c
 * @brief   ILI9341 TFT LCD display driver implementation (SPI interface)
 *
 * This driver uses STM32 HAL SPI for high-speed communication with the
 * ILI9341 TFT controller. All drawing functions use an internal framebuffer
 * approach with set-window-then-flush for maximum throughput.
 */

#include "lcd_ili9341.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* --------------------------------------------------------------------------
 * ILI9341 Command Set
 * -------------------------------------------------------------------------- */
#define ILI9341_NOP         0x00U
#define ILI9341_SWRESET     0x01U
#define ILI9341_RDDID       0x04U
#define ILI9341_RDDST       0x09U
#define ILI9341_SLPIN       0x10U
#define ILI9341_SLPOUT      0x11U
#define ILI9341_PTLON       0x12U
#define ILI9341_NORON       0x13U
#define ILI9341_INVOFF      0x20U
#define ILI9341_INVON       0x21U
#define ILI9341_GAMMASET    0x26U
#define ILI9341_DISPOFF     0x28U
#define ILI9341_DISPON      0x29U
#define ILI9341_CASET       0x2AU
#define ILI9341_PASET       0x2BU
#define ILI9341_RAMWR       0x2CU
#define ILI9341_RAMRD       0x2EU
#define ILI9341_PTLAR       0x30U
#define ILI9341_MADCTL      0x36U
#define ILI9341_PIXFMT      0x3AU
#define ILI9341_FRMCTR1     0xB1U
#define ILI9341_FRMCTR2     0xB2U
#define ILI9341_FRMCTR3     0xB3U
#define ILI9341_INVCTR      0xB4U
#define ILI9341_DFUNCTR     0xB6U
#define ILI9341_PWCTR1      0xC0U
#define ILI9341_PWCTR2      0xC1U
#define ILI9341_PWCTR3      0xC2U
#define ILI9341_PWCTR4      0xC3U
#define ILI9341_PWCTR5      0xC4U
#define ILI9341_VMCTR1      0xC5U
#define ILI9341_VMCTR2      0xC7U
#define ILI9341_RDID1       0xDAU
#define ILI9341_RDID2       0xDBU
#define ILI9341_RDID3       0xDCU
#define ILI9341_RDID4       0xDDU
#define ILI9341_GMCTRP1     0xE0U
#define ILI9341_GMCTRN1     0xE1U

/* --------------------------------------------------------------------------
 * GPIO Pin Definitions (for STM32F407 Discovery board expansion headers)
 *   SPI2_SCK  : PB13 (AF5)
 *   SPI2_MOSI : PB15 (AF5)
 *   LCD_CS    : PE0  (GPIO Output, software-controlled)
 *   LCD_DC    : PE1  (GPIO Output)
 *   LCD_RST   : PE2  (GPIO Output)
 * -------------------------------------------------------------------------- */
#define LCD_CS_PORT         GPIOE
#define LCD_CS_PIN          GPIO_PIN_0
#define LCD_DC_PORT         GPIOE
#define LCD_DC_PIN          GPIO_PIN_1
#define LCD_RST_PORT        GPIOE
#define LCD_RST_PIN         GPIO_PIN_2

/* CS control macros */
#define LCD_CS_LOW()        HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define LCD_CS_HIGH()       HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)

/* DC control macros */
#define LCD_DC_LOW()        HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define LCD_DC_HIGH()       HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)

/* RST control macros */
#define LCD_RST_LOW()       HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define LCD_RST_HIGH()      HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)

/* --------------------------------------------------------------------------
 * Module-level static state
 * -------------------------------------------------------------------------- */
static SPI_HandleTypeDef  *lcd_spi = NULL;
static uint8_t             lcd_rotation = 0; /* 0=portrait, 1=landscape */
static uint16_t            lcd_width  = ILI9341_WIDTH;
static uint16_t            lcd_height = ILI9341_HEIGHT;

/* --------------------------------------------------------------------------
 * 5x7 ASCII Font (characters 0x20 - 0x7F)
 *
 * Each character is 5 bytes: 5 columns, each byte is the 7 rows (bit 0 = top).
 * Bits are packed top-down within each column.
 * Generated from standard 5x7 monospace font glyphs.
 * -------------------------------------------------------------------------- */
static const uint8_t font_5x7[96][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /*  32 ' ' */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /*  33 '!' */
    {0x00, 0x07, 0x00, 0x07, 0x00}, /*  34 '"' */
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, /*  35 '#' */
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /*  36 '$' */
    {0x23, 0x13, 0x08, 0x64, 0x62}, /*  37 '%' */
    {0x36, 0x49, 0x55, 0x22, 0x50}, /*  38 '&' */
    {0x00, 0x05, 0x03, 0x00, 0x00}, /*  39 ''' */
    {0x00, 0x1C, 0x22, 0x41, 0x00}, /*  40 '(' */
    {0x00, 0x41, 0x22, 0x1C, 0x00}, /*  41 ')' */
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, /*  42 '*' */
    {0x08, 0x08, 0x3E, 0x08, 0x08}, /*  43 '+' */
    {0x00, 0x50, 0x30, 0x00, 0x00}, /*  44 ',' */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /*  45 '-' */
    {0x00, 0x60, 0x60, 0x00, 0x00}, /*  46 '.' */
    {0x20, 0x10, 0x08, 0x04, 0x02}, /*  47 '/' */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /*  48 '0' */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /*  49 '1' */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /*  50 '2' */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /*  51 '3' */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /*  52 '4' */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /*  53 '5' */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /*  54 '6' */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /*  55 '7' */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /*  56 '8' */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /*  57 '9' */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /*  58 ':' */
    {0x00, 0x56, 0x36, 0x00, 0x00}, /*  59 ';' */
    {0x00, 0x08, 0x14, 0x22, 0x41}, /*  60 '<' */
    {0x14, 0x14, 0x14, 0x14, 0x14}, /*  61 '=' */
    {0x41, 0x22, 0x14, 0x08, 0x00}, /*  62 '>' */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /*  63 '?' */
    {0x32, 0x49, 0x79, 0x41, 0x3E}, /*  64 '@' */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /*  65 'A' */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /*  66 'B' */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /*  67 'C' */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /*  68 'D' */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /*  69 'E' */
    {0x7F, 0x09, 0x09, 0x01, 0x01}, /*  70 'F' */
    {0x3E, 0x41, 0x41, 0x51, 0x32}, /*  71 'G' */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /*  72 'H' */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /*  73 'I' */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /*  74 'J' */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /*  75 'K' */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /*  76 'L' */
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, /*  77 'M' */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /*  78 'N' */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /*  79 'O' */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /*  80 'P' */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /*  81 'Q' */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /*  82 'R' */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /*  83 'S' */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /*  84 'T' */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /*  85 'U' */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /*  86 'V' */
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, /*  87 'W' */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /*  88 'X' */
    {0x03, 0x04, 0x78, 0x04, 0x03}, /*  89 'Y' */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /*  90 'Z' */
    {0x00, 0x00, 0x7F, 0x41, 0x41}, /*  91 '[' */
    {0x02, 0x04, 0x08, 0x10, 0x20}, /*  92 '\' */
    {0x41, 0x41, 0x7F, 0x00, 0x00}, /*  93 ']' */
    {0x04, 0x02, 0x01, 0x02, 0x04}, /*  94 '^' */
    {0x40, 0x40, 0x40, 0x40, 0x40}, /*  95 '_' */
    {0x00, 0x01, 0x02, 0x04, 0x00}, /*  96 '`' */
    {0x20, 0x54, 0x54, 0x54, 0x78}, /*  97 'a' */
    {0x7F, 0x48, 0x44, 0x44, 0x38}, /*  98 'b' */
    {0x38, 0x44, 0x44, 0x44, 0x20}, /*  99 'c' */
    {0x38, 0x44, 0x44, 0x48, 0x7F}, /* 100 'd' */
    {0x38, 0x54, 0x54, 0x54, 0x18}, /* 101 'e' */
    {0x08, 0x7E, 0x09, 0x01, 0x02}, /* 102 'f' */
    {0x08, 0x14, 0x54, 0x54, 0x3C}, /* 103 'g' */
    {0x7F, 0x08, 0x04, 0x04, 0x78}, /* 104 'h' */
    {0x00, 0x44, 0x7D, 0x40, 0x00}, /* 105 'i' */
    {0x20, 0x40, 0x44, 0x3D, 0x00}, /* 106 'j' */
    {0x00, 0x7F, 0x10, 0x28, 0x44}, /* 107 'k' */
    {0x00, 0x41, 0x7F, 0x40, 0x00}, /* 108 'l' */
    {0x7C, 0x04, 0x18, 0x04, 0x78}, /* 109 'm' */
    {0x7C, 0x08, 0x04, 0x04, 0x78}, /* 110 'n' */
    {0x38, 0x44, 0x44, 0x44, 0x38}, /* 111 'o' */
    {0x7C, 0x14, 0x14, 0x14, 0x08}, /* 112 'p' */
    {0x08, 0x14, 0x14, 0x18, 0x7C}, /* 113 'q' */
    {0x7C, 0x08, 0x04, 0x04, 0x08}, /* 114 'r' */
    {0x48, 0x54, 0x54, 0x54, 0x20}, /* 115 's' */
    {0x04, 0x3F, 0x44, 0x40, 0x20}, /* 116 't' */
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, /* 117 'u' */
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, /* 118 'v' */
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, /* 119 'w' */
    {0x44, 0x28, 0x10, 0x28, 0x44}, /* 120 'x' */
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, /* 121 'y' */
    {0x44, 0x64, 0x54, 0x4C, 0x44}, /* 122 'z' */
    {0x00, 0x08, 0x36, 0x41, 0x00}, /* 123 '{' */
    {0x00, 0x00, 0x7F, 0x00, 0x00}, /* 124 '|' */
    {0x00, 0x41, 0x36, 0x08, 0x00}, /* 125 '}' */
    {0x08, 0x04, 0x08, 0x10, 0x08}, /* 126 '~' */
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* 127 DEL */
};

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/**
 * @brief  Send a command byte to the ILI9341 (DC = LOW).
 */
static void LCD_WriteCommand(uint8_t cmd)
{
    LCD_DC_LOW();
    LCD_CS_LOW();
    HAL_SPI_Transmit(lcd_spi, &cmd, 1, 10);
    LCD_CS_HIGH();
}

/**
 * @brief  Send a data byte to the ILI9341 (DC = HIGH).
 */
static void LCD_WriteData(uint8_t data)
{
    LCD_DC_HIGH();
    LCD_CS_LOW();
    HAL_SPI_Transmit(lcd_spi, &data, 1, 10);
    LCD_CS_HIGH();
}

/**
 * @brief  Send multiple data bytes to the ILI9341 (DC = HIGH).
 */
static void LCD_WriteDataBurst(const uint8_t *data, uint16_t len)
{
    if (len == 0) return;
    LCD_DC_HIGH();
    LCD_CS_LOW();
    HAL_SPI_Transmit(lcd_spi, (uint8_t *)data, len, 100);
    LCD_CS_HIGH();
}

/**
 * @brief  Send a 16-bit color value (split into two bytes).
 */
static void LCD_WriteColor(uint16_t color)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(color >> 8);
    buf[1] = (uint8_t)(color & 0xFF);
    LCD_WriteDataBurst(buf, 2);
}

/**
 * @brief  Send a 16-bit color value repeatedly for fast fills.
 */
static void LCD_WriteColorBurst(uint16_t color, uint32_t count)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(color >> 8);
    buf[1] = (uint8_t)(color & 0xFF);

    LCD_DC_HIGH();
    LCD_CS_LOW();

    for (uint32_t i = 0; i < count; i++) {
        HAL_SPI_Transmit(lcd_spi, buf, 2, 100);
    }

    LCD_CS_HIGH();
}

/**
 * @brief  Hardware reset sequence for the ILI9341.
 */
static void LCD_HardwareReset(void)
{
    LCD_RST_HIGH();
    HAL_Delay(5);
    LCD_RST_LOW();
    HAL_Delay(20);  /* Minimum 10ms reset pulse */
    LCD_RST_HIGH();
    HAL_Delay(150); /* Wait for controller to stabilize */
}

/**
 * @brief  Send the ILI9341 initialization sequence.
 */
static void LCD_InitSequence(void)
{
    /* Software reset */
    LCD_WriteCommand(ILI9341_SWRESET);
    HAL_Delay(5);

    /* Power control A */
    LCD_WriteCommand(ILI9341_PWCTR1);
    LCD_WriteData(0x23);  /* GVDD = 4.6V */

    /* Power control B */
    LCD_WriteCommand(ILI9341_PWCTR2);
    LCD_WriteData(0x10);  /* 0x10 = VGH/VGL with step-up 1 */

    /* VCOM control 1 */
    LCD_WriteCommand(ILI9341_VMCTR1);
    LCD_WriteData(0x2B);  /* VCOMH = 4.25V */
    LCD_WriteData(0x2B);  /* VCOML = -1.5V (actually same value) */

    /* VCOM control 2 */
    LCD_WriteCommand(ILI9341_VMCTR2);
    LCD_WriteData(0xC0);  /* VCOM offset = 0 */

    /* Memory access control */
    LCD_WriteCommand(ILI9341_MADCTL);
    LCD_WriteData(0x48);  /* MX=1, BGR=1 (RGB order) */

    /* Pixel format */
    LCD_WriteCommand(ILI9341_PIXFMT);
    LCD_WriteData(0x55);  /* 16 bits/pixel, 65K colors (RGB565) */

    /* Frame rate control (normal mode) */
    LCD_WriteCommand(ILI9341_FRMCTR1);
    LCD_WriteData(0x00);  /* Division ratio = fosc */
    LCD_WriteData(0x1B);  /* Frame rate = ~70Hz */

    /* Display function control */
    LCD_WriteCommand(ILI9341_DFUNCTR);
    LCD_WriteData(0x0A);  /* Normally white, non-inverted */
    LCD_WriteData(0xA2);  /* V63/VCOM offset, interval scan */

    /* Gamma curve */
    LCD_WriteCommand(ILI9341_GAMMASET);
    LCD_WriteData(0x01);  /* Gamma curve 1 (GC0) */

    /* Positive gamma correction */
    LCD_WriteCommand(ILI9341_GMCTRP1);
    {
        static const uint8_t gamma_P[15] = {
            0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
            0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03,
            0x0E, 0x09, 0x00
        };
        LCD_WriteDataBurst(gamma_P, 15);
    }

    /* Negative gamma correction */
    LCD_WriteCommand(ILI9341_GMCTRN1);
    {
        static const uint8_t gamma_N[15] = {
            0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
            0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C,
            0x31, 0x36, 0x0F
        };
        LCD_WriteDataBurst(gamma_N, 15);
    }

    /* Exit sleep */
    LCD_WriteCommand(ILI9341_SLPOUT);
    HAL_Delay(120);  /* Must wait >= 120ms after sleep out */

    /* Display ON */
    LCD_WriteCommand(ILI9341_DISPON);
    HAL_Delay(20);
}

/* --------------------------------------------------------------------------
 * Public API Implementation
 * -------------------------------------------------------------------------- */

bool LCD_Init(void *hspi)
{
    if (hspi == NULL) {
        return false;
    }

    lcd_spi = (SPI_HandleTypeDef *)hspi;
    lcd_rotation = 0;
    lcd_width  = ILI9341_WIDTH;
    lcd_height = ILI9341_HEIGHT;

    /* Perform hardware reset */
    LCD_HardwareReset();

    /* Send initialization sequence */
    LCD_InitSequence();

    /* Clear the screen to black */
    LCD_FillScreen(COLOR_BLACK);

    return true;
}

void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];

    /* Column address set */
    LCD_WriteCommand(ILI9341_CASET);
    buf[0] = (uint8_t)(x0 >> 8);
    buf[1] = (uint8_t)(x0 & 0xFF);
    buf[2] = (uint8_t)(x1 >> 8);
    buf[3] = (uint8_t)(x1 & 0xFF);
    LCD_WriteDataBurst(buf, 4);

    /* Row address set */
    LCD_WriteCommand(ILI9341_PASET);
    buf[0] = (uint8_t)(y0 >> 8);
    buf[1] = (uint8_t)(y0 & 0xFF);
    buf[2] = (uint8_t)(y1 >> 8);
    buf[3] = (uint8_t)(y1 & 0xFF);
    LCD_WriteDataBurst(buf, 4);

    /* Write to RAM */
    LCD_WriteCommand(ILI9341_RAMWR);
}

void LCD_FillScreen(uint16_t color)
{
    LCD_FillRect(0, 0, lcd_width, lcd_height, color);
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (w == 0 || h == 0) return;
    if (x >= lcd_width || y >= lcd_height) return;

    /* Clip to screen bounds */
    if ((x + w) > lcd_width)  w = lcd_width - x;
    if ((y + h) > lcd_height) h = lcd_height - y;

    LCD_SetWindow(x, y, x + w - 1, y + h - 1);

    uint32_t total_pixels = (uint32_t)w * (uint32_t)h;
    LCD_WriteColorBurst(color, total_pixels);
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= lcd_width || y >= lcd_height) return;

    LCD_SetWindow(x, y, x, y);
    LCD_WriteColor(color);
}

void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    if (x >= lcd_width || y >= lcd_height) return;
    if ((x + len) > lcd_width) len = lcd_width - x;

    LCD_SetWindow(x, y, x + len - 1, y);
    LCD_WriteColorBurst(color, len);
}

void LCD_DrawVLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    if (x >= lcd_width || y >= lcd_height) return;
    if ((y + len) > lcd_height) len = lcd_height - y;

    LCD_SetWindow(x, y, x, y + len - 1);
    LCD_WriteColorBurst(color, len);
}

void LCD_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color,
                  uint16_t bg, FontSize size)
{
    uint8_t char_width, char_height, scale;

    /* Validate character range */
    if (ch < 0x20 || ch > 0x7F) {
        ch = '?';
    }
    ch -= 0x20;

    /* Determine scale factor from font size */
    switch (size) {
        case FONT_SMALL:  scale = 1; break;
        case FONT_MEDIUM: scale = 2; break;
        case FONT_LARGE:  scale = 4; break;
        default:          scale = 1; break;
    }

    char_width  = 6 * scale;  /* 5 columns + 1 spacing column */
    char_height = 8 * scale;  /* 7 rows + 1 descend row */

    /* Don't draw off-screen */
    if (x >= lcd_width || y >= lcd_height) return;

    /* Clip character to screen bounds */
    uint16_t draw_width  = char_width;
    uint16_t draw_height = char_height;
    if ((x + draw_width)  > lcd_width)  draw_width  = lcd_width - x;
    if ((y + draw_height) > lcd_height) draw_height = lcd_height - y;

    for (uint16_t col = 0; col < draw_width; col++) {
        uint8_t src_col = col / scale;
        uint8_t pixel_data;

        if (src_col < 5) {
            pixel_data = font_5x7[(uint8_t)ch][src_col];
        } else {
            pixel_data = 0x00; /* Spacing column is always blank */
        }

        for (uint16_t row = 0; row < draw_height; row++) {
            uint8_t src_row = row / scale;
            uint16_t px_color;

            if (src_row < 7 && (pixel_data & (1U << src_row))) {
                px_color = color;
            } else {
                px_color = bg;
            }

            LCD_DrawPixel(x + col, y + row, px_color);
        }
    }
}

void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color,
                    uint16_t bg, FontSize size)
{
    if (str == NULL) return;

    uint16_t char_w = LCD_CharWidth(size);
    uint16_t cx = x;

    while (*str) {
        /* Handle newline */
        if (*str == '\n') {
            y += LCD_CharHeight(size);
            cx = x;
            str++;
            continue;
        }

        /* Auto-wrap if exceeding screen width */
        if ((cx + char_w) > lcd_width) {
            cx = x;
            y += LCD_CharHeight(size);
            if ((y + LCD_CharHeight(size)) > lcd_height) {
                return; /* Off screen */
            }
        }

        LCD_DrawChar(cx, y, *str, color, bg, size);
        cx += char_w;
        str++;
    }
}

uint16_t LCD_CharWidth(FontSize size)
{
    switch (size) {
        case FONT_SMALL:  return 6;
        case FONT_MEDIUM: return 12;
        case FONT_LARGE:  return 24;
        default:          return 6;
    }
}

uint16_t LCD_CharHeight(FontSize size)
{
    switch (size) {
        case FONT_SMALL:  return 8;
        case FONT_MEDIUM: return 16;
        case FONT_LARGE:  return 32;
        default:          return 8;
    }
}

void LCD_SetRotation(uint8_t rotation)
{
    lcd_rotation = rotation;

    LCD_WriteCommand(ILI9341_MADCTL);

    switch (rotation) {
        case 0: /* Portrait (240x320) */
            LCD_WriteData(0x48);  /* MX=1, BGR=1 */
            lcd_width  = ILI9341_WIDTH;
            lcd_height = ILI9341_HEIGHT;
            break;
        case 1: /* Landscape (320x240) */
            LCD_WriteData(0x28);  /* MV=1, BGR=1 */
            lcd_width  = ILI9341_HEIGHT;
            lcd_height = ILI9341_WIDTH;
            break;
        case 2: /* Portrait inverted */
            LCD_WriteData(0x88);  /* MX=1, MY=1, BGR=1 */
            lcd_width  = ILI9341_WIDTH;
            lcd_height = ILI9341_HEIGHT;
            break;
        case 3: /* Landscape inverted */
            LCD_WriteData(0xE8);  /* MX=1, MV=1, MY=1, BGR=1 */
            lcd_width  = ILI9341_HEIGHT;
            lcd_height = ILI9341_WIDTH;
            break;
        default:
            break;
    }
}
