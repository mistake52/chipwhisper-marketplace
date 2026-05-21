/**
 * hmc5883l.h — HMC5883L 3-axis magnetometer driver
 *
 * I2C address: 0x1E (7-bit), write=0x3C, read=0x3D
 */

#ifndef HMC5883L_H
#define HMC5883L_H

#include <stdint.h>

/* Register map */
#define HMC5883L_REG_CONFIG_A  0x00  /* CRA: sample averaging + data output rate + measurement config */
#define HMC5883L_REG_CONFIG_B  0x01  /* CRB: gain */
#define HMC5883L_REG_MODE       0x02  /* Mode register */
#define HMC5883L_REG_DATA_X_H   0x03  /* X MSB */
#define HMC5883L_REG_STATUS    0x09  /* Status */
#define HMC5883L_REG_ID_A      0x0A  /* ID register A */
#define HMC5883L_REG_ID_B      0x0B  /* ID register B */
#define HMC5883L_REG_ID_C      0x0C  /* ID register C */

/* Expected ID values */
#define HMC5883L_ID_A_VAL      0x48  /* 'H' */
#define HMC5883L_ID_B_VAL      0x34  /* '4' */
#define HMC5883L_ID_C_VAL      0x33  /* '3' */

/* Gain setting (LSB/Gauss) */
typedef enum {
    HMC5883L_GAIN_1370 = 0x00,  /* ±0.88 Ga, 1370 LSB/Gauss */
    HMC5883L_GAIN_1090 = 0x01,  /* ±1.3 Ga,  1090 LSB/Gauss */
    HMC5883L_GAIN_820  = 0x02,  /* ±1.9 Ga,  820 LSB/Gauss  */
    HMC5883L_GAIN_660  = 0x03,  /* ±2.5 Ga,  660 LSB/Gauss  */
    HMC5883L_GAIN_440  = 0x04,  /* ±4.0 Ga,  440 LSB/Gauss  */
    HMC5883L_GAIN_390  = 0x05,  /* ±4.7 Ga,  390 LSB/Gauss  */
    HMC5883L_GAIN_330  = 0x06,  /* ±5.6 Ga,  330 LSB/Gauss  */
    HMC5883L_GAIN_230  = 0x07   /* ±8.1 Ga,  230 LSB/Gauss  */
} hmc5883l_gain_t;

/* Raw magnetometer data */
typedef struct {
    int16_t mx, my, mz;
} hmc5883l_raw_t;

/* Scaled data in micro-Tesla (uT) */
typedef struct {
    float mx, my, mz;  /* uT */
} hmc5883l_scaled_t;

void hmc5883l_init(void);
int hmc5883l_test_connection(void);
void hmc5883l_read_raw(hmc5883l_raw_t *raw);
void hmc5883l_convert(const hmc5883l_raw_t *raw, hmc5883l_scaled_t *scaled);

#endif /* HMC5883L_H */
