/**
 * mpu6050.c — MPU6050 driver implementation
 *
 * Uses I2C1 bus (PB6/SCL, PB7/SDA) at 100kHz.
 * ACCEL_CONFIG: ±2g full scale → 16384 LSB/g
 * GYRO_CONFIG: ±250°/s full scale → 131 LSB/(°/s)
 */

#include "mpu6050.h"
#include "i2c_bus.h"

/* Conversion factors for default full-scale ranges */
#define ACCEL_SENSITIVITY   16384.0f    /* LSB/g for ±2g */
#define GYRO_SENSITIVITY    131.0f      /* LSB/(°/s) for ±250°/s */

int mpu6050_init(void)
{
    uint8_t whoami;

    /* 1. Wake up the MPU6050: write 0x00 to PWR_MGMT_1 */
    i2c_write_reg(MPU6050_ADDR, MPU6050_PWR_MGMT_1, 0x00);

    /* 2. Check WHO_AM_I register */
    whoami = i2c_read_reg(MPU6050_ADDR, MPU6050_WHO_AM_I);
    if (whoami != 0x68) {
        return -1;  /* Device not found or wrong device */
    }

    /* 3. Configure accelerometer: ±2g full scale */
    i2c_write_reg(MPU6050_ADDR, MPU6050_ACCEL_CONFIG, 0x00);

    /* 4. Configure gyroscope: ±250°/s full scale */
    i2c_write_reg(MPU6050_ADDR, MPU6050_GYRO_CONFIG, 0x00);

    /* 5. Set sample rate divider: 0 = 1kHz sample rate (gyro output rate / (1+0)) */
    i2c_write_reg(MPU6050_ADDR, MPU6050_SMPLRT_DIV, 0x00);

    /* 6. Configure DLPF: 0x06 = 5Hz low-pass filter (smoothest) */
    i2c_write_reg(MPU6050_ADDR, MPU6050_CONFIG, 0x06);

    return 0;
}

int mpu6050_whoami(void)
{
    uint8_t id = i2c_read_reg(MPU6050_ADDR, MPU6050_WHO_AM_I);
    return id;
}

void mpu6050_read(mpu6050_data_t *data)
{
    uint8_t buf[14];

    /* Read 14 bytes starting from ACCEL_XOUT_H */
    i2c_read_multi(MPU6050_ADDR, MPU6050_ACCEL_XOUT_H, buf, 14);

    /* Parse accelerometer data (big-endian) */
    data->accel_x = (int16_t)((buf[0] << 8) | buf[1]);
    data->accel_y = (int16_t)((buf[2] << 8) | buf[3]);
    data->accel_z = (int16_t)((buf[4] << 8) | buf[5]);

    /* Temperature (bytes 6-7) — skip */
    /* data->temp = (int16_t)((buf[6] << 8) | buf[7]); */

    /* Parse gyroscope data (big-endian) */
    data->gyro_x = (int16_t)((buf[8] << 8) | buf[9]);
    data->gyro_y = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro_z = (int16_t)((buf[12] << 8) | buf[13]);

    /* Convert to physical units */
    data->accel_x_g  = (float)data->accel_x / ACCEL_SENSITIVITY;
    data->accel_y_g  = (float)data->accel_y / ACCEL_SENSITIVITY;
    data->accel_z_g  = (float)data->accel_z / ACCEL_SENSITIVITY;
    data->gyro_x_dps = (float)data->gyro_x / GYRO_SENSITIVITY;
    data->gyro_y_dps = (float)data->gyro_y / GYRO_SENSITIVITY;
    data->gyro_z_dps = (float)data->gyro_z / GYRO_SENSITIVITY;
}
