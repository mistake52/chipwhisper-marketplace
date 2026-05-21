/**
 * mpu6050.h — MPU6050 6-axis IMU driver (I2C)
 *
 * MPU6050 datasheet register map (general knowledge, not MCP-verified):
 *   I2C address: 0x68 (AD0=0)
 *   WHO_AM_I:      0x75 → returns 0x68
 *   PWR_MGMT_1:    0x6B → write 0x00 to wake up
 *   SMPLRT_DIV:    0x19 → sample rate = gyro_rate / (1 + value)
 *   CONFIG:        0x1A → DLPF config
 *   GYRO_CONFIG:   0x1B → FS_SEL[4:3]: 0=±250°/s
 *   ACCEL_CONFIG:  0x1C → FS_SEL[4:3]: 0=±2g
 *   ACCEL_XOUT_H:  0x3B → accelerometer data (14 bytes contiguously)
 *   TEMP_OUT_H:    0x41
 *   GYRO_XOUT_H:   0x43 → gyroscope data (6 bytes contiguously)
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

#define MPU6050_ADDR        0x68

/* MPU6050 register addresses */
#define MPU6050_WHO_AM_I    0x75
#define MPU6050_PWR_MGMT_1  0x6B
#define MPU6050_SMPLRT_DIV  0x19
#define MPU6050_CONFIG      0x1A
#define MPU6050_GYRO_CONFIG 0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_TEMP_OUT_H   0x41
#define MPU6050_GYRO_XOUT_H  0x43

/* Raw sensor data */
typedef struct {
    int16_t accel_x, accel_y, accel_z;   /* Raw accelerometer */
    int16_t gyro_x, gyro_y, gyro_z;      /* Raw gyroscope */
    float accel_x_g, accel_y_g, accel_z_g;     /* g units */
    float gyro_x_dps, gyro_y_dps, gyro_z_dps;  /* degrees per second */
} mpu6050_data_t;

int  mpu6050_init(void);
int  mpu6050_whoami(void);
void mpu6050_read(mpu6050_data_t *data);

#endif /* MPU6050_H */
