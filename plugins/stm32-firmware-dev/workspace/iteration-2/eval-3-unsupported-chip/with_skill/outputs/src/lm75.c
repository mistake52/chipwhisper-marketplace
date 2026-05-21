/**
 * lm75.c — LM75 I2C temperature sensor driver
 *
 * Register values from general knowledge — NOT MCP-verified.
 *
 * LM75 I2C address: 0x48 (A2=A1=A0=0)
 * Temperature data: 9-bit two's complement, 0.5°C resolution
 *   MSB: bits [8:1] of temperature
 *   LSB: bit   [0]   of temperature (in bit 7 position)
 *
 * Conversion:
 *   int16_t raw = ((int16_t)msb << 3) | ((lsb & 0x80) >> 5);
 *   Temperature in °C = raw * 0.125
 *   Or: raw >> 1 gives temperature * 0.5, so:
 *       whole = raw / 2, fraction = (raw & 1) * 5
 */

#include "main.h"
#include "i2c_bus.h"

/**
 * Read the raw temperature register from LM75.
 *
 * Returns 0 on success (temp_raw filled), negative on I2C error.
 */
int lm75_read_temp(int16_t *temp_raw)
{
    uint8_t buf[2];
    int rc;

    rc = i2c_write_read(LM75_ADDR, LM75_REG_TEMP, buf, 2);
    if (rc < 0) return rc;

    /* Combine MSB and LSB:
     * MSB contains bits [8:1] of the 9-bit temperature value
     * LSB bit 7 contains bit [0]
     *
     * Convert to int16_t: left-align the 9-bit value in a 16-bit field
     * then right-shift to get the fixed-point number.
     *
     * raw16 = (msb << 8) | lsb   → 16-bit with 9 MSbits = temperature
     * Right-shift 7 bits gives 9-bit integer where each unit = 0.5°C
     */
    int16_t raw16 = ((int16_t)buf[0] << 8) | (int16_t)buf[1];
    *temp_raw = raw16 >> 7;  /* Each LSB = 0.5°C */

    return 0;
}

/**
 * Convert raw LM75 value (0.5°C units) to whole + fractional parts.
 *
 * whole:   integer part of °C  (can be negative)
 * tenths:  fractional part in tenths (always 0 or 5)
 */
void lm75_to_celsius(int16_t raw, int *whole, int *tenths)
{
    /* raw is in units of 0.5°C, so divide by 2 */
    if (raw >= 0) {
        *whole  = raw / 2;
        *tenths = (raw & 1) ? 5 : 0;
    } else {
        /* Handle negative: raw is two's complement */
        int neg = -raw;
        *whole  = -(neg / 2);
        *tenths = (neg & 1) ? 5 : 0;
    }
}
