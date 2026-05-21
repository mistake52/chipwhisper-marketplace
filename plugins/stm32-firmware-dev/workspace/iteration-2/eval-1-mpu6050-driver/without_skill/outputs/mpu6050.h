/**
 * @file    mpu6050.h
 * @brief   MPU6050 6-axis IMU register map and driver interface
 *
 * Bare-metal register-level driver for STM32F103C8T6.
 * I2C address: 0x68 (AD0 pin tied low).
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

/*-------------------------------------------------------------------------*/
/* I2C address                                                             */
/*-------------------------------------------------------------------------*/
#define MPU6050_ADDR        0x68  /* 7-bit address, AD0 = GND */

/*-------------------------------------------------------------------------*/
/* Register Map                                                            */
/*-------------------------------------------------------------------------*/
#define MPU6050_REG_WHO_AM_I        0x75  /* Should return 0x68 */
#define MPU6050_REG_SMPLRT_DIV      0x19  /* Sample rate divider */
#define MPU6050_REG_CONFIG          0x1A  /* DLPF config */
#define MPU6050_REG_GYRO_CONFIG     0x1B  /* Gyroscope full-scale range */
#define MPU6050_REG_ACCEL_CONFIG    0x1C  /* Accelerometer full-scale range */
#define MPU6050_REG_PWR_MGMT_1      0x6B  /* Power management 1 */
#define MPU6050_REG_PWR_MGMT_2      0x6C  /* Power management 2 */

/* Accelerometer data (big-endian, 6 bytes) */
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_REG_ACCEL_XOUT_L    0x3C
#define MPU6050_REG_ACCEL_YOUT_H    0x3D
#define MPU6050_REG_ACCEL_YOUT_L    0x3E
#define MPU6050_REG_ACCEL_ZOUT_H    0x3F
#define MPU6050_REG_ACCEL_ZOUT_L    0x40

/* Temperature data (big-endian, 2 bytes) */
#define MPU6050_REG_TEMP_OUT_H      0x41
#define MPU6050_REG_TEMP_OUT_L      0x42

/* Gyroscope data (big-endian, 6 bytes) */
#define MPU6050_REG_GYRO_XOUT_H     0x43
#define MPU6050_REG_GYRO_XOUT_L     0x44
#define MPU6050_REG_GYRO_YOUT_H     0x45
#define MPU6050_REG_GYRO_YOUT_L     0x46
#define MPU6050_REG_GYRO_ZOUT_H     0x47
#define MPU6050_REG_GYRO_ZOUT_L     0x48

/*-------------------------------------------------------------------------*/
/* Configuration values                                                    */
/*-------------------------------------------------------------------------*/
/* PWR_MGMT_1 */
#define MPU6050_PWR_MGMT1_CLKSEL_PLL_X  0x01  /* Clock source: X-axis gyro PLL */
#define MPU6050_PWR_MGMT1_SLEEP         0x40  /* Sleep mode bit */
#define MPU6050_PWR_MGMT1_RESET         0x80  /* Device reset */

/* ACCEL_CONFIG: ±2g full scale */
#define MPU6050_ACCEL_FS_2G             0x00  /* AFS_SEL = 0 -> 16384 LSB/g */
#define MPU6050_ACCEL_FS_4G             0x08
#define MPU6050_ACCEL_FS_8G             0x10
#define MPU6050_ACCEL_FS_16G            0x18

/* GYRO_CONFIG: ±250 deg/s full scale */
#define MPU6050_GYRO_FS_250             0x00  /* FS_SEL = 0 -> 131 LSB/(deg/s) */
#define MPU6050_GYRO_FS_500             0x08
#define MPU6050_GYRO_FS_1000            0x10
#define MPU6050_GYRO_FS_2000            0x18

/* DLPF bandwidth */
#define MPU6050_DLPF_260HZ              0x00  /* Accel: 260Hz, Gyro: 256Hz, delay: 0.98ms */
#define MPU6050_DLPF_184HZ              0x01
#define MPU6050_DLPF_94HZ               0x02
#define MPU6050_DLPF_44HZ               0x03
#define MPU6050_DLPF_21HZ               0x04
#define MPU6050_DLPF_10HZ               0x05
#define MPU6050_DLPF_5HZ                0x06

/*-------------------------------------------------------------------------*/
/* Sensitivity constants (for ±2g accel, ±250dps gyro)                     */
/*-------------------------------------------------------------------------*/
#define MPU6050_ACCEL_SENSITIVITY       16384.0f  /* LSB per g */
#define MPU6050_GYRO_SENSITIVITY        131.0f    /* LSB per deg/s */

/*-------------------------------------------------------------------------*/
/* Driver functions                                                        */
/*-------------------------------------------------------------------------*/

/**
 * @brief  Initialize I2C1 peripheral for MPU6050 communication.
 *         Configures PB6 (SCL) and PB7 (SDA) as alternate-function
 *         open-drain outputs. Sets I2C1 to 100 kHz standard mode.
 */
void mpu6050_i2c_init(void);

/**
 * @brief  Write a single byte to an MPU6050 register.
 * @param  reg   Register address (7-bit)
 * @param  data  Value to write
 */
void mpu6050_write_reg(uint8_t reg, uint8_t data);

/**
 * @brief  Read a single byte from an MPU6050 register.
 * @param  reg   Register address (7-bit)
 * @return Register value
 */
uint8_t mpu6050_read_reg(uint8_t reg);

/**
 * @brief  Read multiple consecutive bytes starting from a register.
 * @param  reg    Starting register address
 * @param  buf    Output buffer
 * @param  len    Number of bytes to read
 */
void mpu6050_read_burst(uint8_t reg, uint8_t *buf, uint8_t len);

/**
 * @brief  Initialize MPU6050: wake up, set ±2g accel, ±250dps gyro,
 *         5 Hz DLPF bandwidth.
 * @return 0 on success, -1 if WHO_AM_I check fails
 */
int  mpu6050_init(void);

/**
 * @brief  Read raw sensor data.
 * @param  accel_x  [out] Raw accelerometer X (16-bit signed)
 * @param  accel_y  [out] Raw accelerometer Y
 * @param  accel_z  [out] Raw accelerometer Z
 * @param  gyro_x   [out] Raw gyroscope X
 * @param  gyro_y   [out] Raw gyroscope Y
 * @param  gyro_z   [out] Raw gyroscope Z
 */
void mpu6050_read_all(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z,
                      int16_t *gyro_x,  int16_t *gyro_y,  int16_t *gyro_z);

#endif /* MPU6050_H */
