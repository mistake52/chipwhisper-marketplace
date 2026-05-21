/**
 * main.c — MSPM0G3507 Temperature-Controlled Fan (Bare-Metal Register Programming)
 *
 * ============================================================================
 * MANUAL ONLY — Phase -1 Compatibility Report
 * ============================================================================
 *   Docs MCP:  NO — embedded-docs MCP only indexes STM32F103C8.
 *                     MSPM0G3507 is a TI Arm Cortex-M0+ chip, not in the STM32
 *                     knowledge base. No MCP-verified register values available.
 *   Sim MCP:   NO — QEMU only emulates STM32F1xx/F4xx. No MSPM0 machine exists.
 *   Quadrant:  MANUAL ONLY — All register values from web research
 *              (TI SLAU846 TRM, SLASEX6 datasheet, TI E2E forum).
 *              Code CANNOT be tested until real MSPM0G3507 hardware is available.
 *
 * ACTION FOR USER: Consider adding MSPM0G3507 SVD + reference manual to the
 * knowledge graph pipeline to enable MCP-based register verification.
 * ============================================================================
 *
 * FUNCTION:
 *   Reads the internal temperature sensor via ADC0 channel 11 every 500ms.
 *   Turns fan ON (PA0 high) when temperature rises above 30°C.
 *   Turns fan OFF (PA0 low) when temperature drops below 25°C.
 *   Implements 5°C hysteresis to prevent oscillation.
 *
 * BUILD:
 *   arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -O2 -g ...
 *
 * HARDWARE:
 *   - LP-MSPM0G3507 LaunchPad (or equivalent MSPM0G3507 board)
 *   - Fan connected to PA0 via MOSFET/BJT driver (do not drive directly)
 *   - 3.3V power supply (VDDA as ADC reference)
 *
 * REGISTER SOURCES (not MCP-verified):
 *   - SLAU846B: MSPM0 G-Series 80-MHz Microcontrollers Technical Reference Manual
 *   - SLASEX6:  MSPM0G350x Mixed-Signal Microcontrollers Datasheet
 *   - TI E2E Forum: Confirmed VDDA=3.3V for factory calibration (not 1.4V)
 */

#include "main.h"

/*===========================================================================
 * SysTick-based delay (Cortex-M0+ built-in timer)
 * Counts down from reload value to 0 at MCLK frequency.
 *===========================================================================*/

/* SysTick registers (Cortex-M0+ core peripheral, same across all CM0+) */
#define SYSTICK_BASE    0xE000E010UL
#define SYSTICK_CSR     (*(volatile uint32_t *)(SYSTICK_BASE + 0x00))
#define SYSTICK_RVR     (*(volatile uint32_t *)(SYSTICK_BASE + 0x04))
#define SYSTICK_CVR     (*(volatile uint32_t *)(SYSTICK_BASE + 0x08))

#define SYSTICK_CSR_ENABLE     (1U << 0)
#define SYSTICK_CSR_TICKINT    (1U << 1)
#define SYSTICK_CSR_CLKSOURCE  (1U << 2)
#define SYSTICK_CSR_COUNTFLAG  (1U << 16)

void delay_init(void) {
    /* Configure SysTick for 1ms period using MCLK (32 MHz) */
    SYSTICK_RVR = SYSTICK_LOAD_1MS;       /* Reload value for 1ms */
    SYSTICK_CVR = 0;                      /* Clear current value */
    SYSTICK_CSR = SYSTICK_CSR_ENABLE | SYSTICK_CSR_CLKSOURCE; /* Enable, MCLK source */
}

void delay_ms(uint32_t ms) {
    while (ms--) {
        /* Wait for COUNTFLAG to be set (timer wrapped to 0) */
        while (!(SYSTICK_CSR & SYSTICK_CSR_COUNTFLAG)) {
            /* spin */
        }
    }
}

/*===========================================================================
 * GPIO initialization — PA0 as output for fan control
 *===========================================================================*/
void gpio_init(void) {
    /* 1. Enable GPIOA power via PWREN register */
    GPIOA_PWREN = PWREN_KEY_ENABLE;       /* KEY=0x26, ENABLE=1 */
    /* Wait >= 4 ULPCLK cycles (~125ns at 32MHz; a few NOPs is sufficient) */
    __asm volatile ("nop; nop; nop; nop; nop; nop; nop; nop;");

    /* 2. Set PA0 as output (DOE31_0 bit 0 = 1) */
    GPIOA_DOE31_0 = (1U << FAN_PIN);

    /* 3. Start with fan OFF */
    GPIOA_DOUTCLR = (1U << FAN_PIN);
}

void fan_on(void) {
    GPIOA_DOUTSET = (1U << FAN_PIN);
}

void fan_off(void) {
    GPIOA_DOUTCLR = (1U << FAN_PIN);
}

/*===========================================================================
 * ADC0 initialization — temperature sensor on channel 11
 *===========================================================================*/
void adc_init(void) {
    uint32_t cal_code;

    /* 1. Enable ADC0 power via PWREN register */
    ADC0_PWREN = PWREN_KEY_ENABLE;
    __asm volatile ("nop; nop; nop; nop; nop; nop; nop; nop;");

    /*
     * 2. Configure ADC clock (CLKCFG)
     *    Default SAMPCLK = ULPCLK (reset value). ULPCLK defaults to SYSOSC/1 = 32MHz.
     *    We leave CLKCFG at default (0x00) — adequate for temp sensing.
     */

    /*
     * 3. Configure CTL0:
     *    - SCLKDIV = /8  → sample clock = 32MHz/8 = 4MHz (0.25µs period)
     *    - PWRDN = AUTO  → ADC auto-powers-down between conversions (saves power)
     */
    ADC0_CTL0 = ADC_CTL0_SCLKDIV_DIV8 | ADC_CTL0_PWRDN_AUTO;

    /*
     * 4. Configure SCOMP0 for 25µs sampling time:
     *    Tsample = SCOMP0 * (1 / (SAMPCLK/SCLKDIV))
     *            = SCOMP0 * (1 / 4MHz) = SCOMP0 * 0.25µs
     *    For 25µs: SCOMP0 = 25 / 0.25 = 100
     *    (Temperature sensor requires minimum 12.5µs; 25µs adds margin.)
     */
    ADC0_SCOMP0 = 100;

    /*
     * 5. Configure CTL2:
     *    - RES = 12-bit
     *    - STARTADD = 0 (use MEMCTL[0])
     *    - ENDADD = 0 (single channel, so same as start)
     */
    ADC0_CTL2 = ADC_CTL2_RES_12BIT;

    /*
     * 6. Configure MEMCTL[0] for temperature sensor:
     *    - CHANSEL = 11      (internal temperature sensor)
     *    - VRSEL = VDD (0)   (VDDA = 3.3V reference)
     *    - STIME = SCOMP0    (use SCOMP0 for sampling time)
     *    - AVGEN = 0         (no hardware averaging — keep it simple)
     *    - TRIG = 0          (auto-trigger — no trigger needed in single mode)
     *    - BCSEN = 0         (no burn-out current source)
     *    - WINCOMP = 0       (no window comparator)
     */
    ADC0_MEMCTL0 = (ADC_CHANNEL_TEMPSENSE << ADC_MEMCTL_CHANSEL_Pos)
                 | ADC_MEMCTL_VRSEL_VDD
                 | ADC_MEMCTL_STIME_SCOMP0;

    /*
     * 7. Read factory temperature calibration value.
     *    TEMP_SENSE0.DATA at 0x41C4003C contains the 12-bit ADC code
     *    measured at TSTRIM = 30°C with VDDA = 3.3V reference.
     */
    cal_code = FACTORY_TEMP_SENSE0;
    (void)cal_code;  /* used in adc_read_temperature() */
}

/*===========================================================================
 * ADC0 single conversion on channel 11 (temperature sensor)
 *===========================================================================*/
uint32_t adc_read_raw(void) {
    /*
     * 1. Enable conversions (set ENC bit in CTL1)
     *    CTL1 bit 1 = ENC
     */
    ADC0_CTL1 = (1U << ADC_CTL1_ENC_Pos)
              | ADC_CTL1_CONSEQ_SINGLE
              | ADC_CTL1_TRIGSRC_SW
              | ADC_CTL1_SAMPMODE_AUTO;

    /*
     * 2. Start conversion (set SC bit in CTL1 while ENC stays set)
     *    CTL1 bit 0 = SC, bit 1 = ENC
     */
    ADC0_CTL1 = (1U << ADC_CTL1_SC_Pos)
              | (1U << ADC_CTL1_ENC_Pos)
              | ADC_CTL1_CONSEQ_SINGLE
              | ADC_CTL1_TRIGSRC_SW
              | ADC_CTL1_SAMPMODE_AUTO;

    /*
     * 3. Wait for conversion to complete.
     *    STATUS bit 0 = BUSY — polls 0 when done.
     */
    while (ADC0_STATUS & ADC_STATUS_BUSY_Msk) {
        /* spin */
    }

    /*
     * 4. Read conversion result from the aliased MEMRES[0] region.
     *    Using MCLK-speed path (0x40556000) as recommended by TI.
     *    Mask to 12 bits because MEMRES may contain status flags in upper bits.
     */
    uint32_t result = ADC0_MEMRES0 & 0xFFF;

    /*
     * 5. Disable conversions (clear ENC) to prepare for next cycle.
     */
    ADC0_CTL1 = 0;

    return result;
}

/*===========================================================================
 * Temperature calculation — integer fixed-point math (no FPU needed)
 *===========================================================================*/

/**
 * Convert ADC code to temperature in millidegrees Celsius.
 *
 * Formula (from SLAU846 TRM, Section 2.2.5):
 *   VTRIM   = (VDDA / 4096) * (CAL_CODE - 0.5)
 *   VSAMPLE = (VDDA / 4096) * (ADC_CODE - 0.5)
 *   T = TSTRIM + (VSAMPLE - VTRIM) / TSc
 *
 * Where TSc = -1.8 mV/°C  (negative = voltage drops as temp rises).
 *
 * Simplified for integer math (mV domain):
 *   delta_mv = (3300 * (ADC_CODE - CAL_CODE)) / 4096
 *   temp_mC  = 30000 - (delta_mv * 5) / 9
 *
 * (Because 1000 / 1800 = 5/9, and TSTRIM = 30°C = 30000 m°C.)
 */
int32_t adc_to_temperature_mC(uint32_t adc_code) {
    uint32_t cal_code = FACTORY_TEMP_SENSE0;

    /*
     * Only the lower 12 bits of the factory register contain the ADC code.
     * The upper bits may contain status/flags depending on device revision.
     */
    cal_code &= 0xFFF;

    /*
     * delta_mv = 3300 * (adc_code - cal_code) / 4096
     * Use int32_t because delta can be negative (hotter → adc_code < cal_code).
     */
    int32_t delta_mv = (int32_t)(3300 * ((int32_t)adc_code - (int32_t)cal_code)) / 4096;

    /*
     * temp_mC = 30000 - (delta_mv * 5) / 9
     * The minus sign correctly accounts for the negative temp coefficient:
     *   hotter → lower ADC reading → negative delta_mv → temp rises.
     */
    int32_t temp_mC = 30000 - (delta_mv * 5) / 9;

    return temp_mC;
}

/*===========================================================================
 * Main application
 *===========================================================================*/
int main(void) {
    uint32_t raw_adc;
    int32_t  temp_mC;
    uint8_t  fan_state = 0;  /* 0 = off, 1 = on */

    /* Initialize hardware */
    delay_init();
    gpio_init();
    adc_init();

    /*
     * Main control loop:
     *   1. Read temperature sensor every ~500ms
     *   2. Turn fan ON  when temperature >  30°C
     *   3. Turn fan OFF when temperature <  25°C
     *   4. 5°C hysteresis prevents oscillation near threshold
     */
    while (1) {
        /* Read temperature sensor */
        raw_adc = adc_read_raw();
        temp_mC = adc_to_temperature_mC(raw_adc);

        /* Fan control with hysteresis */
        if (fan_state == 0) {
            /* Fan is OFF — turn ON if too hot */
            if (temp_mC > (TEMP_ON_THRESHOLD * 1000)) {
                fan_on();
                fan_state = 1;
            }
        } else {
            /* Fan is ON — turn OFF if cool enough */
            if (temp_mC < (TEMP_OFF_THRESHOLD * 1000)) {
                fan_off();
                fan_state = 0;
            }
        }

        /* Wait 500ms before next reading */
        delay_ms(500);
    }

    return 0;  /* never reached */
}
