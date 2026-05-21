#include "ssd1306.h"
#include "spi_driver.h"
#include <math.h>      /* cosf, sinf, atan2f, sqrtf */
#include <string.h>    /* memset */

uint8_t ssd1306_fb[SSD1306_WIDTH * SSD1306_PAGES];

/* 5x7 font: ASCII 32-127, 5 columns per character */
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* 32 space */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /* 33 ! */
    {0x00, 0x07, 0x00, 0x07, 0x00}, /* 34 " */
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, /* 35 # */
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /* 36 $ */
    {0x23, 0x13, 0x08, 0x64, 0x62}, /* 37 % */
    {0x36, 0x49, 0x55, 0x22, 0x50}, /* 38 & */
    {0x00, 0x05, 0x03, 0x00, 0x00}, /* 39 ' */
    {0x00, 0x1C, 0x22, 0x41, 0x00}, /* 40 ( */
    {0x00, 0x41, 0x22, 0x1C, 0x00}, /* 41 ) */
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, /* 42 * */
    {0x08, 0x08, 0x3E, 0x08, 0x08}, /* 43 + */
    {0x00, 0x50, 0x30, 0x00, 0x00}, /* 44 , */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /* 45 - */
    {0x00, 0x60, 0x60, 0x00, 0x00}, /* 46 . */
    {0x20, 0x10, 0x08, 0x04, 0x02}, /* 47 / */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 48 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 49 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 50 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 51 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 52 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 53 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 54 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 55 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 56 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 57 9 */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /* 58 : */
    {0x00, 0x56, 0x36, 0x00, 0x00}, /* 59 ; */
    {0x00, 0x08, 0x14, 0x22, 0x41}, /* 60 < */
    {0x14, 0x14, 0x14, 0x14, 0x14}, /* 61 = */
    {0x41, 0x22, 0x14, 0x08, 0x00}, /* 62 > */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /* 63 ? */
    {0x32, 0x49, 0x79, 0x41, 0x3E}, /* 64 @ */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* 65 A */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* 66 B */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* 67 C */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* 68 D */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* 69 E */
    {0x7F, 0x09, 0x09, 0x01, 0x01}, /* 70 F */
    {0x3E, 0x41, 0x41, 0x51, 0x32}, /* 71 G */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* 72 H */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* 73 I */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* 74 J */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* 75 K */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* 76 L */
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, /* 77 M */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* 78 N */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* 79 O */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* 80 P */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* 81 Q */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* 82 R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* 83 S */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* 84 T */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* 85 U */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* 86 V */
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, /* 87 W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* 88 X */
    {0x03, 0x04, 0x78, 0x04, 0x03}, /* 89 Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* 90 Z */
    {0x00, 0x00, 0x7F, 0x41, 0x41}, /* 91 [ */
    {0x02, 0x04, 0x08, 0x10, 0x20}, /* 92 \ */
    {0x41, 0x41, 0x7F, 0x00, 0x00}, /* 93 ] */
    {0x04, 0x02, 0x01, 0x02, 0x04}, /* 94 ^ */
    {0x40, 0x40, 0x40, 0x40, 0x40}, /* 95 _ */
    {0x00, 0x01, 0x02, 0x04, 0x00}, /* 96 ` */
    {0x20, 0x54, 0x54, 0x54, 0x78}, /* 97 a */
    {0x7F, 0x48, 0x44, 0x44, 0x38}, /* 98 b */
    {0x38, 0x44, 0x44, 0x44, 0x20}, /* 99 c */
    {0x38, 0x44, 0x44, 0x48, 0x7F}, /* 100 d */
    {0x38, 0x54, 0x54, 0x54, 0x18}, /* 101 e */
    {0x08, 0x7E, 0x09, 0x01, 0x02}, /* 102 f */
    {0x08, 0x14, 0x54, 0x54, 0x3C}, /* 103 g */
    {0x7F, 0x08, 0x04, 0x04, 0x78}, /* 104 h */
    {0x00, 0x44, 0x7D, 0x40, 0x00}, /* 105 i */
    {0x20, 0x40, 0x44, 0x3D, 0x00}, /* 106 j */
    {0x00, 0x7F, 0x10, 0x28, 0x44}, /* 107 k */
    {0x00, 0x41, 0x7F, 0x40, 0x00}, /* 108 l */
    {0x7C, 0x04, 0x18, 0x04, 0x78}, /* 109 m */
    {0x7C, 0x08, 0x04, 0x04, 0x78}, /* 110 n */
    {0x38, 0x44, 0x44, 0x44, 0x38}, /* 111 o */
    {0x7C, 0x14, 0x14, 0x14, 0x08}, /* 112 p */
    {0x08, 0x14, 0x14, 0x18, 0x7C}, /* 113 q */
    {0x7C, 0x08, 0x04, 0x04, 0x08}, /* 114 r */
    {0x48, 0x54, 0x54, 0x54, 0x20}, /* 115 s */
    {0x04, 0x3F, 0x44, 0x40, 0x20}, /* 116 t */
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, /* 117 u */
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, /* 118 v */
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, /* 119 w */
    {0x44, 0x28, 0x10, 0x28, 0x44}, /* 120 x */
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, /* 121 y */
    {0x44, 0x64, 0x54, 0x4C, 0x44}, /* 122 z */
    {0x00, 0x08, 0x36, 0x41, 0x00}, /* 123 { */
    {0x00, 0x00, 0x7F, 0x00, 0x00}, /* 124 | */
    {0x00, 0x41, 0x36, 0x08, 0x00}, /* 125 } */
    {0x08, 0x08, 0x2A, 0x1C, 0x08}, /* 126 ~ */
    {0x08, 0x1C, 0x2A, 0x08, 0x08}, /* 127 */
};

/* --- SSD1306 init command sequence --- */
static const uint8_t ssd1306_init_cmds[] = {
    0xAE,       /* Display OFF */
    0xD5, 0x80, /* Set oscillator freq / clock divide */
    0xA8, 0x3F, /* Set multiplex ratio: 64 (0x3F = 63) */
    0xD3, 0x00, /* Set display offset = 0 */
    0x40,       /* Set display start line = 0 */
    0x8D, 0x14, /* Charge pump: enable (0x14 = enable, 0x10 = disable) */
    0x20, 0x00, /* Memory addressing mode: horizontal */
    0xA1,       /* Segment remap: column 127 = SEG0 */
    0xC8,       /* COM output scan direction: remapped (flip vertically) */
    0xDA, 0x12, /* COM pins hardware config: alternative */
    0x81, 0xCF, /* Set contrast: 0xCF */
    0xD9, 0xF1, /* Set pre-charge period: phase1=15, phase2=1 */
    0xDB, 0x40, /* Set VCOMH deselect level */
    0xA4,       /* Entire display ON: resume to RAM content */
    0xA6,       /* Normal display (not inverted) */
    0x2E,       /* Deactivate scrolling */
    0xAF,       /* Display ON */
};

static void ssd1306_write_command(uint8_t cmd) {
    spi1_dc_command();
    spi1_send_byte(cmd);
}

static void ssd1306_write_data(uint8_t data) {
    spi1_dc_data();
    spi1_send_byte(data);
}

void ssd1306_init(void) {
    /* Hardware reset sequence */
    spi1_res_low();
    /* ~10ms delay */
    for (volatile uint32_t i = 0; i < 720000; i++) { __asm__ volatile ("nop"); }
    spi1_res_high();
    /* ~10ms delay */
    for (volatile uint32_t i = 0; i < 720000; i++) { __asm__ volatile ("nop"); }

    /* Send init command sequence */
    for (size_t i = 0; i < sizeof(ssd1306_init_cmds); i++) {
        ssd1306_write_command(ssd1306_init_cmds[i]);
    }

    /* Clear framebuffer and update */
    ssd1306_clear();
    ssd1306_update();
}

void ssd1306_update(void) {
    /* Set column address range: 0-127 */
    ssd1306_write_command(0x21);
    ssd1306_write_command(0x00);
    ssd1306_write_command(127);

    /* Set page address range: 0-7 */
    ssd1306_write_command(0x22);
    ssd1306_write_command(0x00);
    ssd1306_write_command(7);

    /* Send framebuffer data */
    spi1_dc_data();
    spi1_send_bytes(ssd1306_fb, sizeof(ssd1306_fb));
}

void ssd1306_clear(void) {
    memset(ssd1306_fb, 0x00, sizeof(ssd1306_fb));
}

void ssd1306_fill(void) {
    memset(ssd1306_fb, 0xFF, sizeof(ssd1306_fb));
}

void ssd1306_set_pixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;

    uint16_t idx = x + (y / 8) * SSD1306_WIDTH;
    if (color) {
        ssd1306_fb[idx] |= (1U << (y % 8));
    } else {
        ssd1306_fb[idx] &= ~(1U << (y % 8));
    }
}

void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    for (uint8_t yi = 0; yi < h; yi++) {
        for (uint8_t xi = 0; xi < w; xi++) {
            ssd1306_set_pixel(x + xi, y + yi, color);
        }
    }
}

void ssd1306_draw_char(uint8_t x, uint8_t page, char c) {
    if (c < 32 || c > 127) c = ' ';
    if (page >= SSD1306_PAGES) return;

    const uint8_t *glyph = font5x7[c - 32];
    uint16_t fb_idx = page * SSD1306_WIDTH + x;

    for (uint8_t col = 0; col < 5 && (x + col) < SSD1306_WIDTH; col++) {
        ssd1306_fb[fb_idx + col] = glyph[col];
    }
}

void ssd1306_draw_string(uint8_t x, uint8_t page, const char *str) {
    while (*str && x < SSD1306_WIDTH) {
        ssd1306_draw_char(x, page, *str);
        x += 6;
        str++;
    }
}

void ssd1306_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    int dx = (int)x1 - (int)x0;
    int dy = (int)y1 - (int)y0;
    int abs_dx = dx >= 0 ? dx : -dx;
    int abs_dy = dy >= 0 ? dy : -dy;
    int sx = dx >= 0 ? 1 : -1;
    int sy = dy >= 0 ? 1 : -1;
    int err = abs_dx - abs_dy;
    int cx = x0, cy = y0;

    while (1) {
        if (cx >= 0 && (uint8_t)cx < SSD1306_WIDTH &&
            cy >= 0 && (uint8_t)cy < SSD1306_HEIGHT) {
            ssd1306_set_pixel((uint8_t)cx, (uint8_t)cy, 1);
        }
        if (cx == x1 && cy == y1) break;
        int e2 = err * 2;
        if (e2 > -abs_dy) { err -= abs_dy; cx += sx; }
        if (e2 < abs_dx)  { err += abs_dx; cy += sy; }
    }
}

void ssd1306_draw_circle(uint8_t cx, uint8_t cy, uint8_t r) {
    int x = r, y = 0;
    int err = 0;

    while (x >= y) {
        ssd1306_set_pixel(cx + x, cy + y, 1);
        ssd1306_set_pixel(cx + y, cy + x, 1);
        ssd1306_set_pixel(cx - y, cy + x, 1);
        ssd1306_set_pixel(cx - x, cy + y, 1);
        ssd1306_set_pixel(cx - x, cy - y, 1);
        ssd1306_set_pixel(cx - y, cy - x, 1);
        ssd1306_set_pixel(cx + y, cy - x, 1);
        ssd1306_set_pixel(cx + x, cy - y, 1);

        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

void ssd1306_fill_circle(uint8_t cx, uint8_t cy, uint8_t r) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < SSD1306_WIDTH && py >= 0 && py < SSD1306_HEIGHT) {
                    ssd1306_set_pixel((uint8_t)px, (uint8_t)py, 1);
                }
            }
        }
    }
}

void ssd1306_draw_compass(uint8_t cx, uint8_t cy, uint8_t radius, float heading_deg) {
    /* Draw compass circle */
    ssd1306_draw_circle(cx, cy, radius);

    /* Convert heading to radians (0 = North, positive clockwise) */
    float heading_rad = heading_deg * 0.0174533f;  /* deg to rad */

    /* Compass conventions: 0° = up (North), 90° = right (East)
     * Screen coordinates: y increases downward
     * So North = up = negative y in screen coords
     */
    float dx = sinf(heading_rad);   /* East component */
    float dy = -cosf(heading_rad);   /* North component (negative because screen y goes down) */

    /* Arrow tip */
    float tip_x = cx + dx * (radius - 2);
    float tip_y = cy + dy * (radius - 2);

    /* Arrow base points (perpendicular to direction) */
    float perp_x = -dy;
    float perp_y = dx;

    /* Draw arrow tip line */
    ssd1306_draw_line(cx, cy,
        (uint8_t)(tip_x + 0.5f), (uint8_t)(tip_y + 0.5f));

    /* Draw cardinal direction labels */
    ssd1306_draw_char(cx - 2, 1, 'N');
    ssd1306_draw_char(cx - 2, 5, 'S');
    ssd1306_draw_char(cx - radius, 3, 'W');
    ssd1306_draw_char(cx + radius - 5, 3, 'E');
}

void ssd1306_draw_float(uint8_t x, uint8_t page, float val, uint8_t decimals) {
    char buf[16];
    int int_part, frac_part;
    int neg = (val < 0);

    if (neg) val = -val;

    int_part = (int)val;
    frac_part = (int)((val - (float)int_part) * 10.0f + 0.5f); /* only 1 decimal */
    /* Compute actual fractional digits */
    float scale = 1.0f;
    for (uint8_t d = 0; d < decimals; d++) scale *= 10.0f;
    frac_part = (int)((val - (float)int_part) * scale + 0.5f);

    int idx = 0;
    if (neg) buf[idx++] = '-';

    /* Integer part */
    char tmp[8];
    int tmp_idx = 0;
    if (int_part == 0) {
        tmp[tmp_idx++] = '0';
    } else {
        while (int_part > 0) {
            tmp[tmp_idx++] = '0' + (int_part % 10);
            int_part /= 10;
        }
    }
    /* Reverse into buf */
    while (tmp_idx > 0) buf[idx++] = tmp[--tmp_idx];

    if (decimals > 0) {
        buf[idx++] = '.';

        /* Fractional part with leading zeros */
        int div = (int)scale / 10;
        for (uint8_t d = 0; d < decimals; d++) {
            buf[idx++] = '0' + ((frac_part / div) % 10);
            div /= 10;
            if (div == 0) div = 1;
        }
    }
    buf[idx] = '\0';

    ssd1306_draw_string(x, page, buf);
}
