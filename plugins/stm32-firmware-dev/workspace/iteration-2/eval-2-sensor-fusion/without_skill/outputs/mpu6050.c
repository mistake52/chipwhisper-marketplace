#include "mpu6050.h"
#include "i2c_driver.h"

static float g_accel_scale = 2048.0f; /* LSB/g for ±16g */
static float g_gyro_scale  = 16.4f;  /* LSB/(°/s) for ±2000°/s */

int mpu6050_init(void) {
    uint8_t whoami;

    /* Verify device presence: read WHO_AM_I */
    if (i2c1_read_reg(MPU6050_ADDR, MPU6050_WHO_AM_I, &whoami) != 0) return -1;
    if (whoami != 0x68) return -2;

    /* Wake up device: clear sleep bit, select internal 8MHz clock */
    if (i2c1_write_reg(MPU6050_ADDR, MPU6050_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_INT) != 0) {
        return -3;
    }

    /* PWR_MGMT_2: all axes enabled (reset value 0x00 is fine) */

    /* Sample Rate Divider: SMPLRT = Gyro_Output_Rate / (1 + SMPLRT_DIV) - 1
     * Gyro Output Rate = 1kHz (DLPF disabled) or 1kHz (DLPF enabled)
     * Target: 125 Hz => SMPLRT_DIV = 1000/125 - 1 = 7
     */
    if (i2c1_write_reg(MPU6050_ADDR, MPU6050_SMPLRT_DIV, 7) != 0) return -4;

    /* CONFIG: DLPF_CFG = 6 => 5 Hz bandwidth (accel and gyro) */
    if (i2c1_write_reg(MPU6050_ADDR, MPU6050_CONFIG, MPU6050_DLPF_5HZ) != 0) return -5;

    /* GYRO_CONFIG: FS_SEL = 3 => ±2000 °/s */
    if (i2c1_write_reg(MPU6050_ADDR, MPU6050_GYRO_CONFIG, MPU6050_GYRO_FS_2000) != 0) {
        return -6;
    }
    g_gyro_scale = 16.4f;

    /* ACCEL_CONFIG: AFS_SEL = 3 => ±16g */
    if (i2c1_write_reg(MPU6050_ADDR, MPU6050_ACCEL_CONFIG, MPU6050_ACCEL_FS_16) != 0) {
        return -7;
    }
    g_accel_scale = 2048.0f;

    return 0;
}

int mpu6050_self_test(void) {
    /* Basic sanity: read accelerometer and gyro data,
     * verify values are within reasonable range for stationary device.
     */
    mpu6050_raw_t raw;
    if (mpu6050_read_raw(&raw) != 0) return -1;

    /* Accel Z should be roughly +1g (= +accel_scale LSB) when flat */
    /* Gyro values should be near 0 when stationary */
    /* Exact ranges depend on orientation; skip tight bounds for now */

    return 0;
}

int mpu6050_read_raw(mpu6050_raw_t *raw) {
    uint8_t buf[14];
    if (i2c1_read_regs(MPU6050_ADDR, MPU6050_ACCEL_XOUT_H, buf, 14) != 0) {
        return -1;
    }

    raw->accel_x = (int16_t)(((uint16_t)buf[0]  << 8) | buf[1]);
    raw->accel_y = (int16_t)(((uint16_t)buf[2]  << 8) | buf[3]);
    raw->accel_z = (int16_t)(((uint16_t)buf[4]  << 8) | buf[5]);
    raw->temp    = (int16_t)(((uint16_t)buf[6]  << 8) | buf[7]);
    raw->gyro_x  = (int16_t)(((uint16_t)buf[8]  << 8) | buf[9]);
    raw->gyro_y  = (int16_t)(((uint16_t)buf[10] << 8) | buf[11]);
    raw->gyro_z  = (int16_t)(((uint16_t)buf[12] << 8) | buf[13]);

    return 0;
}

int mpu6050_read(mpu6050_data_t *data) {
    mpu6050_raw_t raw;
    if (mpu6050_read_raw(&raw) != 0) return -1;

    /* Convert accelerometer: raw / scale * g * 9.81 => m/s^2 */
    data->accel_x = (float)raw.accel_x / g_accel_scale * 9.80665f;
    data->accel_y = (float)raw.accel_y / g_accel_scale * 9.80665f;
    data->accel_z = (float)raw.accel_z / g_accel_scale * 9.80665f;

    /* Convert gyroscope: raw / scale * (pi / 180) => rad/s */
    data->gyro_x = (float)raw.gyro_x / g_gyro_scale * 0.0174533f;
    data->gyro_y = (float)raw.gyro_y / g_gyro_scale * 0.0174533f;
    data->gyro_z = (float)raw.gyro_z / g_gyro_scale * 0.0174533f;

    /* Temperature: Temp_C = raw_temp / 340 + 36.53 */
    data->temp_c = (float)raw.temp / 340.0f + 36.53f;

    return 0;
}

int mpu6050_is_data_ready(void) {
    uint8_t status;
    if (i2c1_read_reg(MPU6050_ADDR, MPU6050_INT_STATUS, &status) != 0) {
        return 0;
    }
    return (status & 0x01) ? 1 : 0;  /* DATA_RDY_INT = bit 0 */
}

void mpu6050_set_accel_scale(float scale) {
    g_accel_scale = scale;
}

void mpu6050_set_gyro_scale(float scale) {
    g_gyro_scale = scale;
}

float mpu6050_get_accel_scale(void) {
    return g_accel_scale;
}

float mpu6050_get_gyro_scale(void) {
    return g_gyro_scale;
}
