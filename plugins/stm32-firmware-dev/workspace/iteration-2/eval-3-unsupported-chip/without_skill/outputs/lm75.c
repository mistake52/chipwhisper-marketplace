/**
 * @file    lm75.c
 * @brief   LM75 I2C temperature sensor driver implementation
 *
 * Temperature data format (9-bit default):
 *   MSB: Signed integer part (8 bits, two's complement)
 *   LSB: Bit 7 = 0.5°C, bits 6-0 = 0
 *
 *   Example: 0x19 0x40 = +25.5°C
 *            0xFE 0x00 = -2.0°C
 */

#include "lm75.h"

/* HAL headers are expected to be in the include path */
#include "stm32f4xx_hal.h"

/* --------------------------------------------------------------------------
 * Module-level static pointer to I2C handle
 * -------------------------------------------------------------------------- */
static I2C_HandleTypeDef *lm75_i2c = NULL;

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/**
 * @brief  Write one byte to an LM75 register.
 */
static bool LM75_WriteRegister(uint8_t reg, uint16_t value)
{
    HAL_StatusTypeDef status;
    uint8_t data[2];

    if (lm75_i2c == NULL) {
        return false;
    }

    data[0] = (uint8_t)(value >> 8);   /* MSB first */
    data[1] = (uint8_t)(value & 0xFF); /* LSB */

    status = HAL_I2C_Mem_Write(lm75_i2c,
                               LM75_ADDR_8BIT,
                               reg,
                               I2C_MEMADD_SIZE_8BIT,
                               data,
                               2,
                               100);  /* 100ms timeout */

    return (status == HAL_OK);
}

/**
 * @brief  Write one configuration byte to an LM75 register.
 */
static bool LM75_WriteConfig(uint8_t value)
{
    HAL_StatusTypeDef status;

    if (lm75_i2c == NULL) {
        return false;
    }

    status = HAL_I2C_Mem_Write(lm75_i2c,
                               LM75_ADDR_8BIT,
                               LM75_REG_CONFIG,
                               I2C_MEMADD_SIZE_8BIT,
                               &value,
                               1,
                               100);

    return (status == HAL_OK);
}

/* --------------------------------------------------------------------------
 * Public API Implementation
 * -------------------------------------------------------------------------- */

bool LM75_Init(void *hi2c)
{
    if (hi2c == NULL) {
        return false;
    }

    lm75_i2c = (I2C_HandleTypeDef *)hi2c;

    /* Default configuration: normal mode, comparator mode, active low */
    uint8_t config = 0x00;
    return LM75_WriteConfig(config);
}

bool LM75_ReadTemperature(float *temperature)
{
    int16_t raw;

    if (!LM75_ReadTemperatureRaw(&raw) || temperature == NULL) {
        return false;
    }

    /*
     * Raw format (9-bit):
     *   bits 15-7: signed 8-bit integer part
     *   bit 7 of the LSByte: 0.5°C bit
     *
     * We shift right by 7 to normalize the 9-bit data to a signed integer
     * with 1 fractional bit (i.e., multiply by 0.5 later).
     * raw >> 7 gives signed value * 2 (since bit7 of lsb = 0.5°C)
     */
    *temperature = (float)((int16_t)(raw >> 7)) * 0.5f;

    return true;
}

bool LM75_ReadTemperatureRaw(int16_t *raw)
{
    HAL_StatusTypeDef status;
    uint8_t data[2] = {0, 0};

    if (lm75_i2c == NULL || raw == NULL) {
        return false;
    }

    /* Read 2 bytes from the temperature register */
    status = HAL_I2C_Mem_Read(lm75_i2c,
                              LM75_ADDR_8BIT,
                              LM75_REG_TEMP,
                              I2C_MEMADD_SIZE_8BIT,
                              data,
                              2,
                              100);

    if (status != HAL_OK) {
        return false;
    }

    /* Combine MSB and LSB into 16-bit signed value */
    *raw = ((int16_t)data[0] << 8) | (int16_t)data[1];

    return true;
}

bool LM75_SetShutdown(bool shutdown)
{
    uint8_t config;
    HAL_StatusTypeDef status;

    if (lm75_i2c == NULL) {
        return false;
    }

    /* Read current configuration */
    status = HAL_I2C_Mem_Read(lm75_i2c,
                              LM75_ADDR_8BIT,
                              LM75_REG_CONFIG,
                              I2C_MEMADD_SIZE_8BIT,
                              &config,
                              1,
                              100);

    if (status != HAL_OK) {
        return false;
    }

    /* Modify shutdown bit */
    if (shutdown) {
        config |= LM75_CONFIG_SHUTDOWN;
    } else {
        config &= ~LM75_CONFIG_SHUTDOWN;
    }

    return LM75_WriteConfig(config);
}

bool LM75_SetTOS(float temp_threshold)
{
    int16_t raw;
    uint16_t value;

    /* Convert float to LM75 9-bit format */
    if (temp_threshold < -55.0f || temp_threshold > 125.0f) {
        return false;
    }

    /* raw = temp * 256 (9-bit: 8 integer + 1 fractional = multiply by 2) */
    raw = (int16_t)(temp_threshold * 2.0f);
    value = (uint16_t)(raw << 7);  /* Left-align to 9 bits */

    return LM75_WriteRegister(LM75_REG_TOS, value);
}

bool LM75_SetTHYST(float temp_threshold)
{
    int16_t raw;
    uint16_t value;

    if (temp_threshold < -55.0f || temp_threshold > 125.0f) {
        return false;
    }

    raw = (int16_t)(temp_threshold * 2.0f);
    value = (uint16_t)(raw << 7);

    return LM75_WriteRegister(LM75_REG_THYST, value);
}
