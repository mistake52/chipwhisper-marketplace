/**
 * hmc5883l.c — HMC5883L 3-axis magnetometer driver
 *
 * Default configuration:
 *   Sample averaging: 8 samples
 *   Output rate: 75 Hz
 *   Gain: ±1.3 Ga (1090 LSB/Gauss)
 *   Continuous measurement mode
 */

#include "hmc5883l.h"
#include "i2c_bus.h"
#include "main.h"

void hmc5883l_init(void) {
    /* Config A: 8-sample average, 75Hz output rate, normal measurement
     * bits[7:5] = 011 (8 samples averaged)
     * bits[4:2] = 110 (75 Hz output rate)
     * bits[1:0] = 00 (normal measurement config)
     * = 0x78
     */
    i2c_write_reg(HMC5883L_ADDR, HMC5883L_REG_CONFIG_A, 0x78);

    /* Config B: Gain ±1.3 Ga (1090 LSB/Gauss)
     * bits[7:5] = 001
     */
    i2c_write_reg(HMC5883L_ADDR, HMC5883L_REG_CONFIG_B, HMC5883L_GAIN_1090 << 5);

    /* Mode: Continuous measurement
     * bits[1:0] = 00
     */
    i2c_write_reg(HMC5883L_ADDR, HMC5883L_REG_MODE, 0x00);
}

int hmc5883l_test_connection(void) {
    uint8_t id_a, id_b, id_c;
    if (i2c_read_reg(HMC5883L_ADDR, HMC5883L_REG_ID_A, &id_a) != 0) return 0;
    if (i2c_read_reg(HMC5883L_ADDR, HMC5883L_REG_ID_B, &id_b) != 0) return 0;
    if (i2c_read_reg(HMC5883L_ADDR, HMC5883L_REG_ID_C, &id_c) != 0) return 0;
    return (id_a == HMC5883L_ID_A_VAL &&
            id_b == HMC5883L_ID_B_VAL &&
            id_c == HMC5883L_ID_C_VAL);
}

void hmc5883l_read_raw(hmc5883l_raw_t *raw) {
    uint8_t buf[6];
    i2c_read_regs(HMC5883L_ADDR, HMC5883L_REG_DATA_X_H, buf, 6);

    /* X and Z are in reading order, Y and Z are swapped in memory */
    raw->mx = (int16_t)((buf[0] << 8) | buf[1]);
    raw->mz = (int16_t)((buf[2] << 8) | buf[3]);  /* Z comes before Y */
    raw->my = (int16_t)((buf[4] << 8) | buf[5]);
}

void hmc5883l_convert(const hmc5883l_raw_t *raw, hmc5883l_scaled_t *scaled) {
    /* ±1.3 Ga → 1090 LSB/Gauss
     * 1 Gauss = 100 uT
     * scale factor: (1/1090) * 100 = 100/1090 uT/LSB */
    const float scale = 100.0f / 1090.0f;

    scaled->mx = (float)raw->mx * scale;
    scaled->my = (float)raw->my * scale;
    scaled->mz = (float)raw->mz * scale;
}
