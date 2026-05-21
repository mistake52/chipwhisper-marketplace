#include "hmc5883l.h"
#include "i2c_driver.h"

static float g_gain_lsb_per_gauss = 1090.0f; /* Default: ±1.3 Ga */

int hmc5883l_init(void) {
    uint8_t id_a, id_b, id_c;

    /* Read ID registers to verify device presence */
    if (i2c1_read_reg(HMC5883L_ADDR, HMC5883L_ID_A, &id_a) != 0) return -1;
    if (i2c1_read_reg(HMC5883L_ADDR, HMC5883L_ID_B, &id_b) != 0) return -2;
    if (i2c1_read_reg(HMC5883L_ADDR, HMC5883L_ID_C, &id_c) != 0) return -3;

    /* Expected: ID_A='H'(0x48), ID_B='4'(0x34), ID_C='3'(0x33) */
    if (id_a != 0x48 || id_b != 0x34 || id_c != 0x33) return -4;

    /* Config Register A: 8-sample average, 15 Hz output rate, normal bias */
    if (i2c1_write_reg(HMC5883L_ADDR, HMC5883L_CFG_A,
                       HMC5883L_AVG_8 | HMC5883L_RATE_15 | HMC5883L_BIAS_NORMAL) != 0) {
        return -5;
    }

    /* Config Register B: Gain = 1090 LSB/Gauss (±1.3 Ga) */
    if (i2c1_write_reg(HMC5883L_ADDR, HMC5883L_CFG_B, HMC5883L_GAIN_1090) != 0) {
        return -6;
    }
    g_gain_lsb_per_gauss = 1090.0f;

    /* Mode Register: Continuous measurement */
    if (i2c1_write_reg(HMC5883L_ADDR, HMC5883L_MODE, HMC5883L_MODE_CONT) != 0) {
        return -7;
    }

    return 0;
}

int hmc5883l_self_test(void) {
    /* Perform a quick self-check: put in single measurement mode,
     * wait for data ready, read a sample, verify it's non-zero-ish.
     * Real self-test would bias internal straps; this is a basic sanity check.
     */
    uint8_t status;
    /* Switch to single measurement mode */
    if (i2c1_write_reg(HMC5883L_ADDR, HMC5883L_MODE, HMC5883L_MODE_SINGLE) != 0) {
        return -1;
    }

    /* Poll status register for data ready */
    for (volatile uint32_t i = 0; i < 100000; i++) {
        if (i2c1_read_reg(HMC5883L_ADDR, HMC5883L_STATUS, &status) == 0) {
            if (status & HMC5883L_STATUS_RDY) break;
        }
    }

    if (!(status & HMC5883L_STATUS_RDY)) return -2;

    /* Switch back to continuous mode */
    if (i2c1_write_reg(HMC5883L_ADDR, HMC5883L_MODE, HMC5883L_MODE_CONT) != 0) {
        return -3;
    }

    return 0;
}

int hmc5883l_read_raw(hmc5883l_raw_t *raw) {
    uint8_t buf[6];
    if (i2c1_read_regs(HMC5883L_ADDR, HMC5883L_DATA_X_MSB, buf, 6) != 0) {
        return -1;
    }

    /* HMC5883L data order: X MSB, X LSB, Z MSB, Z LSB, Y MSB, Y LSB */
    raw->x = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    raw->z = (int16_t)(((uint16_t)buf[2] << 8) | buf[3]);
    raw->y = (int16_t)(((uint16_t)buf[4] << 8) | buf[5]);

    return 0;
}

int hmc5883l_read(hmc5883l_data_t *data) {
    hmc5883l_raw_t raw;
    if (hmc5883l_read_raw(&raw) != 0) return -1;

    /* Convert LSB to micro-Tesla: value / LSB_per_Gauss * 100 */
    /* 1 Gauss = 100 uT */
    float scale = 100.0f / g_gain_lsb_per_gauss;
    data->x = (float)raw.x * scale;
    data->y = (float)raw.y * scale;
    data->z = (float)raw.z * scale;

    return 0;
}

int hmc5883l_is_data_ready(void) {
    uint8_t status;
    if (i2c1_read_reg(HMC5883L_ADDR, HMC5883L_STATUS, &status) != 0) {
        return 0;
    }
    return (status & HMC5883L_STATUS_RDY) ? 1 : 0;
}

void hmc5883l_set_gain_lsb_per_gauss(float lsb) {
    g_gain_lsb_per_gauss = lsb;
}

float hmc5883l_get_gain_lsb_per_gauss(void) {
    return g_gain_lsb_per_gauss;
}
