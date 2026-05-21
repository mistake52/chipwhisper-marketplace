/**
 * @file    lm75.h
 * @brief   LM75 I2C temperature sensor driver header
 *
 * LM75 is an I2C temperature sensor with 9-bit (0.5°C) or 11-bit (0.125°C)
 * resolution. Default address is 0x48 (A2=A1=A0=GND).
 */

#ifndef LM75_H
#define LM75_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * LM75 I2C Address (7-bit, shifted left by 1 for HAL)
 * A2=A1=A0=GND: 0x48 << 1 = 0x90
 * -------------------------------------------------------------------------- */
#define LM75_ADDR_7BIT         0x48U
#define LM75_ADDR_8BIT         ((LM75_ADDR_7BIT) << 1)

/* --------------------------------------------------------------------------
 * LM75 Register Map
 * -------------------------------------------------------------------------- */
#define LM75_REG_TEMP          0x00U   /* Temperature register (2 bytes, read-only) */
#define LM75_REG_CONFIG        0x01U   /* Configuration register (1 byte, R/W) */
#define LM75_REG_THYST         0x02U   /* Hysteresis register (2 bytes, R/W) */
#define LM75_REG_TOS           0x03U   /* Overtemperature shutdown register (2 bytes, R/W) */

/* --------------------------------------------------------------------------
 * Configuration Register Bits
 * -------------------------------------------------------------------------- */
#define LM75_CONFIG_SHUTDOWN   (1U << 0)  /* 0=Normal, 1=Shutdown */
#define LM75_CONFIG_CMP_INT    (1U << 1)  /* 0=Comparator, 1=Interrupt mode */
#define LM75_CONFIG_OS_POL     (1U << 2)  /* 0=Active low, 1=Active high */
#define LM75_CONFIG_FAULT_MASK (3U << 3)  /* Fault queue: 0=1, 1=2, 2=4, 3=6 faults */

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief  Initialize the LM75 sensor.
 *         Sets up I2C communication and puts the sensor in normal mode.
 * @param  hi2c Handle of the I2C peripheral (HAL I2C_HandleTypeDef*)
 * @return true on success, false on failure
 */
bool LM75_Init(void *hi2c);

/**
 * @brief  Read the temperature from the LM75 sensor.
 * @param  temperature Pointer to store the temperature value in degrees Celsius.
 * @return true on success, false on failure
 */
bool LM75_ReadTemperature(float *temperature);

/**
 * @brief  Read the raw temperature register value.
 * @param  raw Pointer to store the 16-bit raw register value.
 * @return true on success, false on failure
 */
bool LM75_ReadTemperatureRaw(int16_t *raw);

/**
 * @brief  Set the LM75 operating mode.
 * @param  shutdown true for shutdown mode, false for normal mode.
 * @return true on success, false on failure
 */
bool LM75_SetShutdown(bool shutdown);

/**
 * @brief  Set the overtemperature shutdown threshold.
 * @param  temp_threshold Temperature threshold in degrees Celsius.
 * @return true on success, false on failure
 */
bool LM75_SetTOS(float temp_threshold);

/**
 * @brief  Set the hysteresis threshold.
 * @param  temp_threshold Temperature threshold in degrees Celsius.
 * @return true on success, false on failure
 */
bool LM75_SetTHYST(float temp_threshold);

#ifdef __cplusplus
}
#endif

#endif /* LM75_H */
