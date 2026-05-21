/**
 * lcd_ili9341.c — ILI9341 TFT LCD driver (SPI mode)
 *
 * Register values from general knowledge — NOT MCP-verified.
 *
 * Hardware interface: SPI1 + 3 GPIO control pins
 *   SCK  = PA5 (SPI1_SCK,  AF5)
 *   MISO = PA6 (SPI1_MISO, AF5) — not used (display is write-only)
 *   MOSI = PA7 (SPI1_MOSI, AF5)
 *   CS   = PC4 (GPIO output)
 *   DC   = PC5 (GPIO output)  — Data/Command select
 *   RST  = PC6 (GPIO output)
 *
 * NOTE: The STM32F4DISCOVERY (STM32F407G-DISC1) does NOT have a built-in LCD.
 * This driver is provided as a reference. For boards with an ILI9341 (e.g.,
 * STM32F429I-DISC1), adjust pins to match the board's connections.
 *
 * Alternative: For boards without LCD, consider UART output for temperature display.
 */

#include "main.h"
#include "rcc.h"

/* ================================================================
 * GPIO helpers for control pins
 * ================================================================ */
#define LCD_CS_PORT_BASE    GPIOC_BASE
#define LCD_DC_PORT_BASE    GPIOC_BASE
#define LCD_RST_PORT_BASE   GPIOC_BASE

#define GPIO_MODER(port)    (*(volatile uint32_t*)((port) + 0x00))
#define GPIO_ODR(port)      (*(volatile uint32_t*)((port) + 0x14))
#define GPIO_BSRR(port)     (*(volatile uint32_t*)((port) + 0x18))
#define GPIO_AFRL(port)     (*(volatile uint32_t*)((port) + 0x20))

#define MODER_OUTPUT        0x1U
#define MODER_AF            0x2U

/* ================================================================
 * SPI1 Registers (base 0x40013000)
 * ================================================================ */
#define SPI_CR1     (*(volatile uint32_t*)(SPI1_BASE + 0x00))
#define SPI_CR2     (*(volatile uint32_t*)(SPI1_BASE + 0x04))
#define SPI_SR      (*(volatile uint32_t*)(SPI1_BASE + 0x08))
#define SPI_DR      (*(volatile uint32_t*)(SPI1_BASE + 0x0C))

/* SPI_CR1 bits */
#define SPI_CR1_CPHA    (1U << 0)
#define SPI_CR1_CPOL    (1U << 1)
#define SPI_CR1_MSTR    (1U << 2)
#define SPI_CR1_BR_Pos  3
#define SPI_CR1_BR_DIV4 (1U << SPI_CR1_BR_Pos)   /* fPCLK/4 = 21MHz */
#define SPI_CR1_SPE     (1U << 6)
#define SPI_CR1_SSI     (1U << 8)
#define SPI_CR1_SSM     (1U << 9)
#define SPI_CR1_DFF     (1U << 11)   /* 16-bit data frame */

/* SPI_SR bits */
#define SPI_SR_TXE      (1U << 1)
#define SPI_SR_RXNE     (1U << 0)
#define SPI_SR_BSY      (1U << 7)

/* ================================================================
 * ILI9341 Commands
 * ================================================================ */
#define ILI9341_SWRESET   0x01
#define ILI9341_SLPOUT    0x11
#define ILI9341_DISPON    0x29
#define ILI9341_CASET     0x2A
#define ILI9341_PASET     0x2B
#define ILI9341_RAMWR     0x2C
#define ILI9341_MADCTL    0x36
#define ILI9341_COLMOD    0x3A
#define ILI9341_PIXFMT    0x3A

#define LCD_WIDTH   240
#define LCD_HEIGHT  320

/* ================================================================
 * Font — 8x16 pixel, ASCII 32-127
 * See font.h for bitmap data.
 * ================================================================ */
#include "font.h"

/* ================================================================
 * Low-level GPIO helpers
 * ================================================================ */
static inline void lcd_cs_low(void)  { GPIO_BSRR(LCD_CS_PORT_BASE) = (1U << (16 + LCD_CS_PIN)); }
static inline void lcd_cs_high(void) { GPIO_BSRR(LCD_CS_PORT_BASE) = (1U << LCD_CS_PIN); }
static inline void lcd_dc_cmd(void)  { GPIO_BSRR(LCD_DC_PORT_BASE) = (1U << (16 + LCD_DC_PIN)); }
static inline void lcd_dc_data(void) { GPIO_BSRR(LCD_DC_PORT_BASE) = (1U << LCD_DC_PIN); }
static inline void lcd_rst_low(void) { GPIO_BSRR(LCD_RST_PORT_BASE) = (1U << (16 + LCD_RST_PIN)); }
static inline void lcd_rst_high(void){ GPIO_BSRR(LCD_RST_PORT_BASE) = (1U << LCD_RST_PIN); }

/* ================================================================
 * SPI helpers
 * ================================================================ */
static void spi1_send_byte(uint8_t data)
{
    /* Wait for TXE */
    while (!(SPI_SR & SPI_SR_TXE));
    SPI_DR = data;
    /* Wait for RXNE (to clear the RX buffer) */
    while (!(SPI_SR & SPI_SR_RXNE));
    (void)SPI_DR;  /* Discard read byte */
    /* Wait for not busy */
    while (SPI_SR & SPI_SR_BSY);
}

/* ================================================================
 * ILI9341 command/data interface
 * ================================================================ */
static void ili9341_write_cmd(uint8_t cmd)
{
    lcd_cs_low();
    lcd_dc_cmd();
    spi1_send_byte(cmd);
    lcd_cs_high();
}

static void ili9341_write_data(uint8_t data)
{
    lcd_cs_low();
    lcd_dc_data();
    spi1_send_byte(data);
    lcd_cs_high();
}

static void ili9341_write_data16(uint16_t data)
{
    ili9341_write_data((uint8_t)(data >> 8));
    ili9341_write_data((uint8_t)(data & 0xFF));
}

/* ================================================================
 * Set address window for drawing
 * ================================================================ */
static void ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    ili9341_write_cmd(ILI9341_CASET);
    ili9341_write_data16(x0);
    ili9341_write_data16(x1);

    ili9341_write_cmd(ILI9341_PASET);
    ili9341_write_data16(y0);
    ili9341_write_data16(y1);

    ili9341_write_cmd(ILI9341_RAMWR);
}

/* ================================================================
 * Public API
 * ================================================================ */

void lcd_init(void)
{
    /* 1. Enable GPIO clocks and SPI1 clock */
    rcc_enable_gpio();   /* GPIOA+B+C */
    rcc_enable_spi1();   /* SPI1 on APB2 */

    /* 2. Configure SPI1 pins on PA5(SCK), PA6(MISO), PA7(MOSI) as AF5 */
    /* PA5 (SCK) */
    {
        uint32_t moder = GPIO_MODER(GPIOA_BASE);
        moder &= ~(0x3U << (5 * 2));
        moder |= (MODER_AF << (5 * 2));
        GPIO_MODER(GPIOA_BASE) = moder;

        uint32_t afrl = GPIO_AFRL(GPIOA_BASE);
        afrl &= ~(0xFU << (5 * 4));
        afrl |= (5U << (5 * 4));  /* AF5 = SPI1 */
        GPIO_AFRL(GPIOA_BASE) = afrl;
    }
    /* PA7 (MOSI) */
    {
        uint32_t moder = GPIO_MODER(GPIOA_BASE);
        moder &= ~(0x3U << (7 * 2));
        moder |= (MODER_AF << (7 * 2));
        GPIO_MODER(GPIOA_BASE) = moder;

        uint32_t afrl = GPIO_AFRL(GPIOA_BASE);
        afrl &= ~(0xFU << (7 * 4));
        afrl |= (5U << (7 * 4));
        GPIO_AFRL(GPIOA_BASE) = afrl;
    }
    /* PA6 (MISO) — input AF */
    {
        uint32_t moder = GPIO_MODER(GPIOA_BASE);
        moder &= ~(0x3U << (6 * 2));
        moder |= (MODER_AF << (6 * 2));
        GPIO_MODER(GPIOA_BASE) = moder;

        uint32_t afrl = GPIO_AFRL(GPIOA_BASE);
        afrl &= ~(0xFU << (6 * 4));
        afrl |= (5U << (6 * 4));
        GPIO_AFRL(GPIOA_BASE) = afrl;
    }

    /* 3. Configure control pins PC4(CS), PC5(DC), PC6(RST) as outputs */
    uint32_t moder = GPIO_MODER(GPIOC_BASE);
    /* Clear bits for PC4, PC5, PC6 */
    moder &= ~((0x3U << (4*2)) | (0x3U << (5*2)) | (0x3U << (6*2)));
    /* Set as outputs */
    moder |= (MODER_OUTPUT << (4*2)) | (MODER_OUTPUT << (5*2)) | (MODER_OUTPUT << (6*2));
    GPIO_MODER(GPIOC_BASE) = moder;

    /* 4. Configure SPI1:
     * Master mode, fPCLK/4 (21MHz), CPOL=0, CPHA=0, MSB first, software NSS
     */
    SPI_CR1 = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR | SPI_CR1_BR_DIV4 | SPI_CR1_SPE;

    /* 5. Hardware reset the LCD */
    lcd_rst_high();
    delay_ms(1);
    lcd_rst_low();
    delay_ms(10);
    lcd_rst_high();
    delay_ms(120);

    /* 6. ILI9341 initialization sequence */
    ili9341_write_cmd(ILI9341_SWRESET);
    delay_ms(5);

    ili9341_write_cmd(ILI9341_SLPOUT);
    delay_ms(120);

    /* Pixel format: 16-bit (RGB565) */
    ili9341_write_cmd(ILI9341_COLMOD);
    ili9341_write_data(0x55);  /* 16 bits/pixel, MCU interface */
    delay_ms(10);

    /* Memory access control: BGR order, top-to-bottom */
    ili9341_write_cmd(ILI9341_MADCTL);
    ili9341_write_data(0x08);

    /* Display on */
    ili9341_write_cmd(ILI9341_DISPON);
    delay_ms(10);
}

void lcd_clear(uint16_t color)
{
    ili9341_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    lcd_cs_low();
    /* Already in RAMWR mode from set_window — need to re-enter */
    lcd_cs_high();

    /* Re-send RAMWR command then write pixels */
    ili9341_write_cmd(ILI9341_RAMWR);

    lcd_cs_low();
    lcd_dc_data();
    for (uint32_t i = 0; i < (uint32_t)(LCD_WIDTH * LCD_HEIGHT); i++) {
        spi1_send_byte((uint8_t)(color >> 8));
        spi1_send_byte((uint8_t)(color & 0xFF));
    }
    lcd_cs_high();
}

/**
 * Fill a rectangular area with a solid color.
 */
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t x1 = x + w - 1;
    uint16_t y1 = y + h - 1;
    if (x1 >= LCD_WIDTH)  x1 = LCD_WIDTH - 1;
    if (y1 >= LCD_HEIGHT) y1 = LCD_HEIGHT - 1;

    ili9341_set_window(x, y, x1, y1);

    uint32_t pixels = (uint32_t)w * (uint32_t)h;
    lcd_cs_low();
    lcd_dc_data();
    for (uint32_t i = 0; i < pixels; i++) {
        spi1_send_byte((uint8_t)(color >> 8));
        spi1_send_byte((uint8_t)(color & 0xFF));
    }
    lcd_cs_high();
}

/**
 * Draw a single 8x16 character at (x, y).
 */
void lcd_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg)
{
    if (c < 32 || c > 127) c = '?';
    int idx = c - 32;

    ili9341_set_window((uint16_t)x, (uint16_t)y,
                       (uint16_t)(x + 7), (uint16_t)(y + 15));

    lcd_cs_low();
    lcd_dc_data();
    for (int row = 0; row < 16; row++) {
        uint8_t bits = font8x16[idx * 16 + row];
        for (int col = 7; col >= 0; col--) {
            uint16_t color = (bits & (1U << col)) ? fg : bg;
            spi1_send_byte((uint8_t)(color >> 8));
            spi1_send_byte((uint8_t)(color & 0xFF));
        }
    }
    lcd_cs_high();
}

/**
 * Draw a null-terminated string starting at (x, y).
 */
void lcd_draw_string(int x, int y, const char *str, uint16_t fg, uint16_t bg)
{
    while (*str) {
        if (x + 8 > LCD_WIDTH) {
            x = 0;
            y += 16;
        }
        if (y + 16 > LCD_HEIGHT) break;
        lcd_draw_char(x, y, *str, fg, bg);
        x += 8;
        str++;
    }
}

#include <stdio.h>

/**
 * Convenience: clear screen and draw a temperature value.
 */
void lcd_show_temperature(int whole, int tenths)
{
    char buf[32];

    lcd_clear(0x0000);  /* Black background */

    /* Title */
    lcd_draw_string(10, 10, "STM32F407 LM75", 0xFFFF, 0x0000);
    lcd_draw_string(10, 30, "Temperature:", 0x07E0, 0x0000);

    /* Temperature value — large text, centered */
    snprintf(buf, sizeof(buf), "%d.%d C", whole, tenths);
    lcd_draw_string(10, 60, buf, 0xFFE0, 0x0000);  /* Yellow */
}
