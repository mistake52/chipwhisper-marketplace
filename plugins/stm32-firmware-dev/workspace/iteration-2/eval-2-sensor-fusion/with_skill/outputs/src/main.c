/**
 * main.c — E-Compass: HMC5883L + MPU6050 fusion → SSD1306 OLED
 *
 * STM32F103C8 bare-metal firmware (no HAL).
 *
 * Pin Assignments (MCP-verified):
 *   I2C1:  PB6=SCL, PB7=SDA         → MPU6050 (0x68) + HMC5883L (0x1E)
 *   SPI1:  PA5=SCK, PA7=MOSI        → SSD1306 OLED
 *   GPIO:  PB0=OLED_RES, PB1=OLED_DC  (output push-pull)
 *
 * Peripherals:
 *   I2C1 @ 0x40005400  (APB1, 100kHz standard mode)
 *   SPI1 @ 0x40013000  (APB2, 9MHz, CPOL=0 CPHA=0)
 *
 * Algorithm: Madgwick AHRS (9-DOF MARG) → yaw (0-360 degree compass heading)
 *
 * Build:
 *   make all              (hardware: HSE 8MHz → PLL×9 → 72MHz)
 *   make SIMULATION=1 all (QEMU: HSI 8MHz, no PLL)
 */

#include "main.h"
#include "rcc.h"
#include "systick.h"
#include "i2c_bus.h"
#include "spi_bus.h"
#include "mpu6050.h"
#include "hmc5883l.h"
#include "ssd1306.h"
#include "madgwick.h"
#include "font5x7.h"
#include <math.h>
#include <stdio.h>

/* Buffer for sprintf output */
static char line_buf[32];

/* Draw text helpers using the 5x7 font */
static void oled_draw_char(uint8_t x, uint8_t y, char c, uint8_t invert) {
    if (c < FONT5X7_START) c = FONT5X7_START;
    uint8_t idx = c - FONT5X7_START;
    if (idx >= FONT5X7_COUNT) idx = 0;

    for (uint8_t col = 0; col < FONT5X7_WIDTH; col++) {
        uint8_t col_data = font5x7[idx][col];
        for (uint8_t row = 0; row < FONT5X7_HEIGHT; row++) {
            uint8_t color = (col_data >> row) & 0x01;
            if (invert) color = !color;
            ssd1306_draw_pixel(x + col, y + row, color);
        }
    }
}

static void oled_draw_string(uint8_t x, uint8_t y, const char *str, uint8_t invert) {
    uint8_t cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y += FONT5X7_HEIGHT + 1;
        } else {
            oled_draw_char(cx, y, *str, invert);
            cx += FONT5X7_WIDTH + 1;
        }
        str++;
    }
}

static void oled_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    int16_t dx = (int16_t)x1 - x0;
    int16_t dy = (int16_t)y1 - y0;
    int16_t sx = dx > 0 ? 1 : -1;
    int16_t sy = dy > 0 ? 1 : -1;
    dx = dx > 0 ? dx : -dx;
    dy = dy > 0 ? dy : -dy;
    int16_t err = (dx > dy ? dx : -dy) / 2;
    int16_t e2;

    while (1) {
        ssd1306_draw_pixel(x0, y0, 1);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

/**
 * Draw compass rose heading indicator on OLED.
 */
static void draw_compass(float heading) {
    int16_t cx = 64, cy = 32;  /* center */
    int16_t r = 28;             /* radius */

    /* Draw circle outline */
    for (int16_t i = 0; i < 360; i += 2) {
        float rad = (float)i * M_PI / 180.0f;
        int16_t px = cx + (int16_t)(r * sinf(rad));
        int16_t py = cy - (int16_t)(r * cosf(rad));
        if (px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT) {
            ssd1306_draw_pixel(px, py, 1);
        }
    }

    /* Draw cardinal ticks (N, E, S, W at 0, 90, 180, 270 deg) */
    for (int16_t deg = 0; deg < 360; deg += 90) {
        float rad = (float)deg * M_PI / 180.0f;
        int16_t x1 = cx + (int16_t)((r - 2) * sinf(rad));
        int16_t y1 = cy - (int16_t)((r - 2) * cosf(rad));
        int16_t x2 = cx + (int16_t)((r + 2) * sinf(rad));
        int16_t y2 = cy - (int16_t)((r + 2) * cosf(rad));
        oled_draw_line(x1, y1, x2, y2);
    }

    /* Draw heading needle */
    float h_rad = (heading - 90.0f) * M_PI / 180.0f;
    int16_t nx = cx + (int16_t)((r - 4) * cosf(h_rad));
    int16_t ny = cy + (int16_t)((r - 4) * sinf(h_rad));
    oled_draw_line(cx, cy, nx, ny);

    /* Show heading as text */
    snprintf(line_buf, sizeof(line_buf), "HDG:%03.0f", heading);
    oled_draw_string(0, 0, line_buf, 0);

    /* N/E/S/W labels */
    oled_draw_char(cx - 2, cy - r - 7, 'N', 0);
    oled_draw_char(cx + r + 1, cy - 3, 'E', 0);
    oled_draw_char(cx - 2, cy + r + 1, 'S', 0);
    oled_draw_char(cx - r - 6, cy - 3, 'W', 0);
}

/**
 * Main entry point.
 */
int main(void) {
    /* Phase 1: System initialization */
    rcc_init();
    systick_init();

    /* Phase 2: Peripheral initialization */
    i2c_init();
    spi_init();
    ssd1306_init();

    /* Phase 3: Sensor initialization */
    mpu6050_init();
    hmc5883l_init();

    /* Phase 4: Sensor self-test (WHO_AM_I registers) */
    /* On real hardware, check return values. In simulation, sensors won't respond. */
    int mpu_ok = mpu6050_test_connection();
    int hmc_ok = hmc5883l_test_connection();

    /* Phase 5: Madgwick filter initialization (100Hz, beta=0.1) */
    madgwick_init(100.0f, 0.1f);

    /* Main loop */
    uint32_t last_sample = millis();
    float heading = 0.0f;

    while (1) {
        if ((millis() - last_sample) >= SENSOR_DT_MS) {
            last_sample = millis();

            /* Read sensors */
            mpu6050_raw_t  imu_raw;
            hmc5883l_raw_t mag_raw;

            mpu6050_read_raw(&imu_raw);
            hmc5883l_read_raw(&mag_raw);

            /* Convert to physical units */
            mpu6050_scaled_t  imu;
            hmc5883l_scaled_t mag;
            mpu6050_convert(&imu_raw, &imu);
            hmc5883l_convert(&mag_raw, &mag);

            /* Run Madgwick AHRS fusion */
            madgwick_update(imu.gx, imu.gy, imu.gz,
                           imu.ax, imu.ay, imu.az,
                           mag.mx, mag.my, mag.mz);

            /* Extract yaw (compass heading) */
            euler_t euler;
            madgwick_get_euler(&euler);
            heading = euler.yaw;

            /* Update OLED display */
            ssd1306_clear();
            draw_compass(heading);

            /* Show status lines */
            if (mpu_ok) {
                oled_draw_string(0, 56, "MPU OK", 0);
            } else {
                oled_draw_string(0, 56, "MPU --", 0);
            }

            if (hmc_ok) {
                oled_draw_string(48, 56, "HMC OK", 0);
            } else {
                oled_draw_string(48, 56, "HMC --", 0);
            }

            ssd1306_update();
        }

        __asm__ volatile ("wfi");
    }

    return 0;
}

/**
 * Default fault handler — trap in infinite loop.
 * In QEMU, this catches unhandled interrupts / faults.
 */
void Default_Handler(void) {
    while (1) {
        __asm__ volatile ("nop");
    }
}
