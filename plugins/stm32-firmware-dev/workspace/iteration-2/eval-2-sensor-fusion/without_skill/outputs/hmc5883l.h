#ifndef HMC5883L_H
#define HMC5883L_H

#include <stdint.h>

/* HMC5883L I2C address (7-bit) */
#define HMC5883L_ADDR       0x1E

/* HMC5883L Registers */
#define HMC5883L_CFG_A      0x00  /* Config Register A */
#define HMC5883L_CFG_B      0x01  /* Config Register B (gain) */
#define HMC5883L_MODE       0x02  /* Mode Register */
#define HMC5883L_DATA_X_MSB 0x03  /* X MSB */
#define HMC5883L_DATA_X_LSB 0x04  /* X LSB */
#define HMC5883L_DATA_Z_MSB 0x05  /* Z MSB */
#define HMC5883L_DATA_Z_LSB 0x06  /* Z LSB */
#define HMC5883L_DATA_Y_MSB 0x07  /* Y MSB */
#define HMC5883L_DATA_Y_LSB 0x08  /* Y LSB */
#define HMC5883L_STATUS     0x09  /* Status Register */
#define HMC5883L_ID_A       0x0A  /* ID Register A */
#define HMC5883L_ID_B       0x0B  /* ID Register B */
#define HMC5883L_ID_C       0x0C  /* ID Register C */

/* CFG_A: MA1:MA0 = samples averaged, DO2:DO0 = data output rate */
#define HMC5883L_AVG_1       (0x00 << 5)
#define HMC5883L_AVG_2       (0x01 << 5)
#define HMC5883L_AVG_4       (0x02 << 5)
#define HMC5883L_AVG_8       (0x03 << 5)
#define HMC5883L_RATE_0_75   (0x00 << 2)
#define HMC5883L_RATE_1_5    (0x01 << 2)
#define HMC5883L_RATE_3      (0x02 << 2)
#define HMC5883L_RATE_7_5    (0x03 << 2)
#define HMC5883L_RATE_15     (0x04 << 2)
#define HMC5883L_RATE_30     (0x05 << 2)
#define HMC5883L_RATE_75     (0x06 << 2)
#define HMC5883L_BIAS_NORMAL (0x00 << 0)

/* CFG_B: GN2:GN0 = gain */
#define HMC5883L_GAIN_1370   (0x00 << 5)  /* ±0.88 Ga, 1370 LSB/Gauss */
#define HMC5883L_GAIN_1090   (0x01 << 5)  /* ±1.3 Ga,  1090 LSB/Gauss */
#define HMC5883L_GAIN_820    (0x02 << 5)  /* ±1.9 Ga,  820 LSB/Gauss */
#define HMC5883L_GAIN_660    (0x03 << 5)  /* ±2.5 Ga,  660 LSB/Gauss */
#define HMC5883L_GAIN_440    (0x04 << 5)  /* ±4.0 Ga,  440 LSB/Gauss */
#define HMC5883L_GAIN_390    (0x05 << 5)  /* ±4.7 Ga,  390 LSB/Gauss */
#define HMC5883L_GAIN_330    (0x06 << 5)  /* ±5.6 Ga,  330 LSB/Gauss */
#define HMC5883L_GAIN_230    (0x07 << 5)  /* ±8.1 Ga,  230 LSB/Gauss */

/* MODE */
#define HMC5883L_MODE_CONT   0x00
#define HMC5883L_MODE_SINGLE 0x01
#define HMC5883L_MODE_IDLE   0x02  /* actually 0x02 or 0x03 */

/* Status */
#define HMC5883L_STATUS_RDY  (1U << 0)
#define HMC5883L_STATUS_LOCK (1U << 1)

/* Raw magnetometer reading (16-bit signed) */
typedef struct {
    int16_t x;
    int16_t z;  /* Note: HMC5883L data order is X, Z, Y */
    int16_t y;
} hmc5883l_raw_t;

/* Calibrated reading in micro-Tesla (uT) */
typedef struct {
    float x;
    float y;
    float z;
} hmc5883l_data_t;

int  hmc5883l_init(void);
int  hmc5883l_self_test(void);
int  hmc5883l_read_raw(hmc5883l_raw_t *raw);
int  hmc5883l_read(hmc5883l_data_t *data);
int  hmc5883l_is_data_ready(void);
void hmc5883l_set_gain_lsb_per_gauss(float lsb);
float hmc5883l_get_gain_lsb_per_gauss(void);

#endif /* HMC5883L_H */
