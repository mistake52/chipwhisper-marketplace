/**
 * mpu6050.c — MPU-6050 6-axis IMU driver
 *
 * Default configuration:
 *   Gyro:  ±2000 deg/s (sensitivity = 16.4 LSB/deg/s)
 *   Accel: ±2g (sensitivity = 16384 LSB/g)
 *   DLPF:  42Hz (sample rate = 1kHz / (1 + SMPLRT_DIV))
 */

#include "mpu6050.h"
#include "i2c_bus.h"
#include "main.h"
#include <math.h>

#define MPU6050_DEG2RAD (M_PI / 180.0f)

void mpu6050_init(void) {
    /* Wake up MPU6050 (clear sleep bit in PWR_MGMT_1) */
    i2c_write_reg(MPU6050_ADDR, MPU6050_REG_PWR_MGMT_1, 0x00);

    /* Set sample rate divider:
     * Sample Rate = Gyro Output Rate / (1 + SMPLRT_DIV)
     * Gyro Output Rate = 8kHz (when DLPF disabled) or 1kHz (with DLPF)
     * With DLPF 42Hz: SR = 1000 / (1 + 9) = 100Hz
     */
    i2c_write_reg(MPU6050_ADDR, MPU6050_REG_SMPLRT_DIV, 9);

    /* Set DLFP_CFG = 3 (42Hz bandwidth for gyro and accel) */
    i2c_write_reg(MPU6050_ADDR, MPU6050_REG_CONFIG, 0x03);

    /* Set gyro full scale ±2000 deg/s */
    i2c_write_reg(MPU6050_ADDR, MPU6050_REG_GYRO_CONFIG, MPU6050_GYRO_2000 << 3);

    /* Set accel full scale ±2g */
    i2c_write_reg(MPU6050_ADDR, MPU6050_REG_ACCEL_CONFIG, MPU6050_ACCEL_2G << 3);
}

int mpu6050_test_connection(void) {
    uint8_t whoami;
    if (i2c_read_reg(MPU6050_ADDR, MPU6050_REG_WHO_AM_I, &whoami) != 0) {
        return 0;
    }
    return (whoami == MPU6050_WHO_AM_I_VAL);
}

void mpu6050_read_raw(mpu6050_raw_t *raw) {
    uint8_t buf[14];
    i2c_read_regs(MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT_H, buf, 14);

    raw->accel_x = (int16_t)((buf[0] << 8) | buf[1]);
    raw->accel_y = (int16_t)((buf[2] << 8) | buf[3]);
    raw->accel_z = (int16_t)((buf[4] << 8) | buf[5]);
    raw->temp    = (int16_t)((buf[6] << 8) | buf[7]);
    raw->gyro_x  = (int16_t)((buf[8] << 8) | buf[9]);
    raw->gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
    raw->gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);
}

void mpu6050_convert(const mpu6050_raw_t *raw, mpu6050_scaled_t *scaled) {
    /* ±2g → 16384 LSB/g → 1g = 16384 */
    scaled->ax = (float)raw->accel_x / 16384.0f;
    scaled->ay = (float)raw->accel_y / 16384.0f;
    scaled->az = (float)raw->accel_z / 16384.0f;

    /* ±2000 deg/s → 16.4 LSB/(deg/s) */
    /* Convert to rad/s */
    scaled->gx = (float)raw->gyro_x / 16.4f * MPU6050_DEG2RAD;
    scaled->gy = (float)raw->gyro_y / 16.4f * MPU6050_DEG2RAD;
    scaled->gz = (float)raw->gyro_z / 16.4f * MPU6050_DEG2RAD;
}
