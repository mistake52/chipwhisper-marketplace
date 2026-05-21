/**
 * main.c — STM32F407VGT6 Discovery: LM75 I2C Temperature Sensor + LCD Display
 *
 * Register values from general knowledge — NOT MCP-verified.
 * (STM32F4xx is not in the embedded-docs knowledge base; only F103 is covered.)
 *
 * Chip compatibility:
 *   Docs MCP:  UNSUPPORTED (only STM32F103C8 is indexed)
 *   Sim  MCP:  SUPPORTED  (STM32F407VG → netduinoplus2 QEMU machine)
 *
 * This firmware falls in the "Sim-only" quadrant:
 *   - Register values from general knowledge (F4 reference manuals)
 *   - Can be verified in QEMU simulation (phase 2)
 *   - MCP-verified register values NOT available
 *
 * Hardware:
 *   - Board: STM32F407VGT6 Discovery (STM32F407G-DISC1)
 *   - LM75 I2C temperature sensor on I2C1 (PB6=SCL, PB7=SDA)
 *   - ILI9341 LCD via SPI1 (PA5=SCK, PA7=MOSI, PC4=CS, PC5=DC, PC6=RST)
 *
 * NOTE: The STM32F4DISCOVERY does NOT actually have a built-in LCD.
 * If no LCD is present, the SPI output is harmless and UART can be used
 * as an alternative display method.
 */

#include "main.h"
#include "rcc.h"
#include "systick.h"
#include "i2c_bus.h"
#include "lm75.h"
#include "lcd_ili9341.h"

#include <stdio.h>

int main(void)
{
    int16_t temp_raw;
    int whole, tenths;
    char buf[64];

    /* ---- System initialization ---- */
    rcc_init();           /* Configure 168MHz system clock */
    systick_init();       /* 1ms SysTick timer */
    i2c_init();           /* I2C1 @ 100kHz on PB6/PB7 */
    lcd_init();           /* ILI9341 LCD via SPI1 */

    /* ---- Splash screen ---- */
    lcd_clear(0x0000);  /* Black background */
    lcd_draw_string(10, 10,  "STM32F407VGT6", 0x07E0, 0x0000);
    lcd_draw_string(10, 30,  "LM75 I2C Temp", 0x07E0, 0x0000);
    lcd_draw_string(10, 50,  "Initializing...", 0xFFFF, 0x0000);

    delay_ms(1000);

    /* ---- Main loop: read temperature and display every 500ms ---- */
    while (1) {
        int rc = lm75_read_temp(&temp_raw);

        lcd_clear(0x0000);

        if (rc == 0) {
            lm75_to_celsius(temp_raw, &whole, &tenths);

            /* Line 1: Title */
            lcd_draw_string(10, 10, "STM32F407 LM75", 0xFFFF, 0x0000);
            lcd_draw_string(10, 28, "----------------", 0x8410, 0x0000);

            /* Line 2: Temperature label */
            lcd_draw_string(10, 48, "Temperature:", 0x07E0, 0x0000);

            /* Line 3: Temperature value — large/centered */
            snprintf(buf, sizeof(buf), "  %d.%d C", whole, tenths);
            lcd_draw_string(10, 72, buf, 0xFFE0, 0x0000);

            /* Line 4: Raw value (debug) */
            snprintf(buf, sizeof(buf), "raw: %d (0.5C)", (int)temp_raw);
            lcd_draw_string(10, 96, buf, 0x8410, 0x0000);

            /* Line 5: Status */
            lcd_draw_string(10, 120, "I2C OK", 0x07E0, 0x0000);

            /* Temperature range indicator */
            if (whole < 0) {
                lcd_draw_string(10, 144, "Status: COLD", 0x041F, 0x0000);
            } else if (whole < 20) {
                lcd_draw_string(10, 144, "Status: COOL", 0x07FF, 0x0000);
            } else if (whole < 30) {
                lcd_draw_string(10, 144, "Status: WARM", 0xFFE0, 0x0000);
            } else {
                lcd_draw_string(10, 144, "Status: HOT!", 0xF800, 0x0000);
            }
        } else {
            /* I2C communication error */
            lcd_draw_string(10, 10, "LM75 ERROR", 0xF800, 0x0000);
            lcd_draw_string(10, 30, "I2C comm fail", 0xFFFF, 0x0000);
            snprintf(buf, sizeof(buf), "err code: %d", rc);
            lcd_draw_string(10, 50, buf, 0xFFFF, 0x0000);
            lcd_draw_string(10, 80, "Check wiring:", 0x8410, 0x0000);
            lcd_draw_string(10, 100, "PB6=SCL PB7=SDA", 0x8410, 0x0000);
            lcd_draw_string(10, 120, "A2=A1=A0=GND", 0x8410, 0x0000);
        }

        delay_ms(500);
    }

    return 0;  /* Unreachable */
}
