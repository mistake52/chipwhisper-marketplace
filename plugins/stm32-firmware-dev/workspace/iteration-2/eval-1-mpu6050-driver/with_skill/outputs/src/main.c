/**
 * main.c — MPU6050 driver demo for STM32F103C8T6
 *
 * Reads accelerometer (g) and gyroscope (°/s) from MPU6050 via I2C1,
 * prints to USART1 (PA9 TX) at 1Hz.
 *
 * Pin wiring:
 *   MPU6050 VCC → STM32 3.3V
 *   MPU6050 GND → STM32 GND
 *   MPU6050 SCL → STM32 PB6
 *   MPU6050 SDA → STM32 PB7
 */

#include "main.h"
#include "rcc.h"
#include "systick.h"
#include "uart.h"
#include "i2c_bus.h"
#include "mpu6050.h"

int main(void)
{
    int ret;
    mpu6050_data_t sensor;

    /* Phase 0: Initialize hardware */
    rcc_init();         /* HSE 8MHz → PLL x9 → 72MHz */
    systick_init();     /* 1ms SysTick */
    uart_init();        /* USART1 115200 8N1 */
    i2c_init();         /* I2C1 100kHz */

    /* Phase 1: Initialize MPU6050 */
    ret = mpu6050_init();
    if (ret != 0) {
        uart_puts("MPU6050 init FAILED!\r\n");
        while (1) {
            /* Blink LED or just hang */
        }
    }

    uart_puts("MPU6050 Driver Ready\r\n");
    uart_puts("Accel(g): X, Y, Z | Gyro(dps): X, Y, Z\r\n");
    uart_puts("----------------------------------------\r\n");

    /* Phase 2: Main loop — read sensor every 1 second */
    while (1) {
        mpu6050_read(&sensor);

        /* Print accelerometer data in g */
        uart_puts("Accel: ");
        uart_print_float(sensor.accel_x_g, 2);
        uart_puts(", ");
        uart_print_float(sensor.accel_y_g, 2);
        uart_puts(", ");
        uart_print_float(sensor.accel_z_g, 2);

        uart_puts("  |  Gyro: ");
        uart_print_float(sensor.gyro_x_dps, 2);
        uart_puts(", ");
        uart_print_float(sensor.gyro_y_dps, 2);
        uart_puts(", ");
        uart_print_float(sensor.gyro_z_dps, 2);
        uart_puts("\r\n");

        delay_ms(1000);
    }

    return 0;
}
