/**
 * mpu6050.h — MPU-6050 6-axis IMU driver (gyroscope + accelerometer)
 *
 * I2C address: 0x68 (AD0=GND), write=0xD0, read=0xD1
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

/* Register map */
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_WHO_AM_I     0x75

/* Accelerometer data (high byte first) */
#define MPU6050_REG_ACCEL_XOUT_H 0x3B

/* WHO_AM_I expected value */
#define MPU6050_WHO_AM_I_VAL     0x68

/* Gyro full-scale range (deg/s) */
typedef enum {
    MPU6050_GYRO_250  = 0x00,  /* ±250 deg/s  */
    MPU6050_GYRO_500  = 0x01,  /* ±500 deg/s  */
    MPU6050_GYRO_1000 = 0x02,  /* ±1000 deg/s */
    MPU6050_GYRO_2000 = 0x03   /* ±2000 deg/s */
} mpu6050_gyro_range_t;

/* Accelerometer full-scale range (g) */
typedef enum {
    MPU6050_ACCEL_2G  = 0x00,
    MPU6050_ACCEL_4G  = 0x01,
    MPU6050_ACCEL_8G  = 0x02,
    MPU6050_ACCEL_16G = 0x03
} mpu6050_accel_range_t;

/* Raw sensor data */
typedef struct {
    int16_t accel_x, accel_y, accel_z;  /* LSB */
    int16_t gyro_x,  gyro_y,  gyro_z;   /* LSB */
    int16_t temp;                        /* LSB */
} mpu6050_raw_t;

/* Scaled sensor data (float) */
typedef struct {
    float ax, ay, az;   /* m/s^2 or g (depending on calibration) */
    float gx, gy, gz;   /* rad/s */
} mpu6050_scaled_t;

void mpu6050_init(void);
int mpu6050_test_connection(void);
void mpu6050_read_raw(mpu6050_raw_t *raw);
void mpu6050_convert(const mpu6050_raw_t *raw, mpu6050_scaled_t *scaled);

#endif /* MPU6050_H */
