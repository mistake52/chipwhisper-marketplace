/**
 * ssd1306.c — SSD1306 128x64 OLED display driver (SPI mode)
 *
 * GPIO control pins (MCP-verified: GPIOB @ 0x40010C00):
 *   PB0 = RES (output PP)
 *   PB1 = DC  (output PP)
 *
 * OLED control:
 *   DC=0 → command byte
 *   DC=1 → data byte
 *   RES=0 → hardware reset
 */

#include "ssd1306.h"
#include "spi_bus.h"
#include "main.h"

/* GPIOB register definitions (MCP-verified) */
#define GPIOB_BASE  0x40010C00UL
#define GPIOB_CRL   (*(volatile uint32_t*)(GPIOB_BASE + 0x00))
#define GPIOB_BSRR  (*(volatile uint32_t*)(GPIOB_BASE + 0x10))
#define GPIOB_BRR   (*(volatile uint32_t*)(GPIOB_BASE + 0x14))

#define OLED_RES_PIN  0
#define OLED_DC_PIN   1
#define OLED_RES_BIT  (1UL << OLED_RES_PIN)
#define OLED_DC_BIT   (1UL << OLED_DC_PIN)

/* Framebuffer: 128 × 64 = 1024 bytes, 8 vertical pages */
static uint8_t fb[128 * 8];

/* Internal helpers */
static void ssd1306_write_cmd(uint8_t cmd) {
    GPIOB_BRR = OLED_DC_BIT;   /* DC = 0 (command) */
    spi_write(cmd);
}

static void ssd1306_write_data(uint8_t data) {
    GPIOB_BSRR = OLED_DC_BIT;  /* DC = 1 (data) */
    spi_write(data);
}

void ssd1306_init(void) {
    /* Configure PB0 (RES) and PB1 (DC) as GPIO push-pull output
     * CNF=00 MODE=11 → 4-bit value 0x3 */
    uint32_t crl = GPIOB_CRL;
    /* PB0: bits 3:0, PB1: bits 7:4 */
    crl &= ~((0xFUL << 0) | (0xFUL << 4));
    crl |= (0x3UL << 0) | (0x3UL << 4);
    GPIOB_CRL = crl;

    /* Hardware reset sequence */
    GPIOB_BSRR = OLED_RES_BIT;   /* RES high */
    for (volatile int i = 0; i < 1000; i++) { __asm__ volatile ("nop"); }
    GPIOB_BRR = OLED_RES_BIT;    /* RES low */
    for (volatile int i = 0; i < 1000; i++) { __asm__ volatile ("nop"); }
    GPIOB_BSRR = OLED_RES_BIT;   /* RES high */
    for (volatile int i = 0; i < 1000; i++) { __asm__ volatile ("nop"); }

    /* SSD1306 initialization sequence */
    ssd1306_write_cmd(0xAE);  /* Display OFF */

    ssd1306_write_cmd(0xD5);  /* Set display clock divide ratio/oscillator frequency */
    ssd1306_write_cmd(0x80);  /* Default: 0x80 */

    ssd1306_write_cmd(0xA8);  /* Set multiplex ratio */
    ssd1306_write_cmd(0x3F);  /* 64 lines */

    ssd1306_write_cmd(0xD3);  /* Set display offset */
    ssd1306_write_cmd(0x00);  /* No offset */

    ssd1306_write_cmd(0x40);  /* Set display start line to 0 */

    ssd1306_write_cmd(0x8D);  /* Charge pump setting */
    ssd1306_write_cmd(0x14);  /* Enable charge pump (0x14) */

    ssd1306_write_cmd(0x20);  /* Memory addressing mode */
    ssd1306_write_cmd(0x00);  /* Horizontal addressing mode */

    ssd1306_write_cmd(0xA1);  /* Segment remap: column 127 mapped to SEG0 */

    ssd1306_write_cmd(0xC8);  /* COM output scan direction: remapped mode */

    ssd1306_write_cmd(0xDA);  /* COM pins hardware configuration */
    ssd1306_write_cmd(0x12);  /* Alternative COM pin config */

    ssd1306_write_cmd(0x81);  /* Set contrast control */
    ssd1306_write_cmd(0xCF);  /* Contrast value */

    ssd1306_write_cmd(0xD9);  /* Set pre-charge period */
    ssd1306_write_cmd(0xF1);  /* Phase 1: 15 DCLK, Phase 2: 1 DCLK */

    ssd1306_write_cmd(0xDB);  /* Set VCOMH deselect level */
    ssd1306_write_cmd(0x40);  /* ~0.77 × VCC */

    ssd1306_write_cmd(0xA4);  /* Entire display ON: follow RAM content */

    ssd1306_write_cmd(0xA6);  /* Normal display (not inverted) */

    ssd1306_write_cmd(0x2E);  /* Deactivate scroll */

    ssd1306_write_cmd(0xAF);  /* Display ON */

    /* Clear framebuffer and display */
    ssd1306_clear();
    ssd1306_update();
}

void ssd1306_clear(void) {
    for (uint16_t i = 0; i < sizeof(fb); i++) {
        fb[i] = 0x00;
    }
}

void ssd1306_update(void) {
    /* Write entire framebuffer using horizontal addressing mode */
    ssd1306_write_cmd(0x21);  /* Set column address range */
    ssd1306_write_cmd(0);     /* Start column 0 */
    ssd1306_write_cmd(127);   /* End column 127 */
    ssd1306_write_cmd(0x22);  /* Set page address range */
    ssd1306_write_cmd(0);     /* Start page 0 */
    ssd1306_write_cmd(7);     /* End page 7 */

    /* Send all 1024 bytes of framebuffer */
    for (uint16_t i = 0; i < sizeof(fb); i++) {
        ssd1306_write_data(fb[i]);
    }
}

void ssd1306_draw_pixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;

    uint16_t byte_idx = x + (y / 8) * OLED_WIDTH;
    uint8_t bit_mask = 1 << (y & 0x07);

    if (color) {
        fb[byte_idx] |= bit_mask;
    } else {
        fb[byte_idx] &= ~bit_mask;
    }
}

void ssd1306_set_contrast(uint8_t contrast) {
    ssd1306_write_cmd(0x81);
    ssd1306_write_cmd(contrast);
}
