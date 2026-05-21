#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

/* MPU6050 I2C address (7-bit, AD0=GND) */
#define MPU6050_ADDR          0x68

/* MPU6050 Registers */
#define MPU6050_SMPLRT_DIV    0x19  /* Sample Rate Divider */
#define MPU6050_CONFIG         0x1A  /* Configuration (DLPF, EXT_SYNC) */
#define MPU6050_GYRO_CONFIG    0x1B  /* Gyroscope Configuration */
#define MPU6050_ACCEL_CONFIG   0x1C  /* Accelerometer Configuration */
#define MPU6050_FIFO_EN        0x23  /* FIFO Enable */
#define MPU6050_I2C_MST_CTRL   0x24  /* I2C Master Control */
#define MPU6050_INT_PIN_CFG    0x37  /* INT Pin / Bypass Enable */
#define MPU6050_INT_ENABLE     0x38  /* Interrupt Enable */
#define MPU6050_INT_STATUS     0x3A  /* Interrupt Status */

/* Data registers (big-endian 16-bit) */
#define MPU6050_ACCEL_XOUT_H   0x3B
#define MPU6050_ACCEL_XOUT_L   0x3C
#define MPU6050_ACCEL_YOUT_H   0x3D
#define MPU6050_ACCEL_YOUT_L   0x3E
#define MPU6050_ACCEL_ZOUT_H   0x3F
#define MPU6050_ACCEL_ZOUT_L   0x40
#define MPU6050_TEMP_OUT_H     0x41
#define MPU6050_TEMP_OUT_L     0x42
#define MPU6050_GYRO_XOUT_H    0x43
#define MPU6050_GYRO_XOUT_L    0x44
#define MPU6050_GYRO_YOUT_H    0x45
#define MPU6050_GYRO_YOUT_L    0x46
#define MPU6050_GYRO_ZOUT_H    0x47
#define MPU6050_GYRO_ZOUT_L    0x48

#define MPU6050_PWR_MGMT_1     0x6B  /* Power Management 1 */
#define MPU6050_PWR_MGMT_2     0x6C  /* Power Management 2 */
#define MPU6050_WHO_AM_I       0x75  /* Who Am I (should be 0x68) */

/* PWR_MGMT_1 bits */
#define MPU6050_PWR1_DEVICE_RESET  (1U << 7)
#define MPU6050_PWR1_SLEEP         (1U << 6)
#define MPU6050_PWR1_CYCLE         (1U << 5)
#define MPU6050_PWR1_TEMP_DIS      (1U << 3)
#define MPU6050_PWR1_CLKSEL_INT    (0x00 << 0)  /* Internal 8MHz */

/* GYRO_CONFIG FS_SEL */
#define MPU6050_GYRO_FS_250    (0x00 << 3)  /* ±250 °/s,  131 LSB/°/s */
#define MPU6050_GYRO_FS_500    (0x01 << 3)  /* ±500 °/s,  65.5 LSB/°/s */
#define MPU6050_GYRO_FS_1000   (0x02 << 3)  /* ±1000 °/s, 32.8 LSB/°/s */
#define MPU6050_GYRO_FS_2000   (0x03 << 3)  /* ±2000 °/s, 16.4 LSB/°/s */

/* ACCEL_CONFIG AFS_SEL */
#define MPU6050_ACCEL_FS_2     (0x00 << 3)  /* ±2g,  16384 LSB/g */
#define MPU6050_ACCEL_FS_4     (0x01 << 3)  /* ±4g,  8192 LSB/g */
#define MPU6050_ACCEL_FS_8     (0x02 << 3)  /* ±8g,  4096 LSB/g */
#define MPU6050_ACCEL_FS_16    (0x03 << 3)  /* ±16g, 2048 LSB/g */

/* DLPF_CFG (in CONFIG register) */
#define MPU6050_DLPF_256HZ     0x00  /* 8kHz sample, 256Hz bandwidth */
#define MPU6050_DLPF_188HZ     0x01
#define MPU6050_DLPF_98HZ      0x02
#define MPU6050_DLPF_42HZ      0x03
#define MPU6050_DLPF_20HZ      0x04
#define MPU6050_DLPF_10HZ      0x05
#define MPU6050_DLPF_5HZ       0x06

/* Raw sensor reading */
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temp;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} mpu6050_raw_t;

/* Calibrated reading */
typedef struct {
    float accel_x;     /* m/s^2 */
    float accel_y;
    float accel_z;
    float gyro_x;      /* rad/s */
    float gyro_y;
    float gyro_z;
    float temp_c;      /* degrees Celsius */
} mpu6050_data_t;

int  mpu6050_init(void);
int  mpu6050_self_test(void);
int  mpu6050_read_raw(mpu6050_raw_t *raw);
int  mpu6050_read(mpu6050_data_t *data);
int  mpu6050_is_data_ready(void);
void mpu6050_set_accel_scale(float scale);
void mpu6050_set_gyro_scale(float scale);
float mpu6050_get_accel_scale(void);
float mpu6050_get_gyro_scale(void);

#endif /* MPU6050_H */
