/**
 * @file    main.c
 * @brief   Temperature-Controlled Fan Firmware for MSPM0G3507
 *
 * Bare-metal register-level implementation.
 * Reads the internal temperature sensor via ADC0 and controls a
 * fan connected to PA0:
 *   - Temperature > 30°C  → Fan ON  (PA0 HIGH)
 *   - Temperature < 25°C  → Fan OFF (PA0 LOW)
 *   - Hysteresis zone (25-30°C): keeps current state
 *
 * Hardware: MSPM0G3507 (ARM Cortex-M0+, 80 MHz max)
 * Toolchain: arm-none-eabi-gcc
 */

#include <stdint.h>

/* --------------------------------------------------------------------------
 * Register Definitions
 * -------------------------------------------------------------------------- */

/* --- SYSCTL (System Controller) ----------------------------------------- */
#define SYSCTL_BASE        0x40080000UL
#define SYSCTL_READBUSY    (*(volatile uint32_t *)(SYSCTL_BASE + 0x0000))
#define SYSCTL_IIDX         (*(volatile uint32_t *)(SYSCTL_BASE + 0x0008))
#define SYSCTL_RESET_STAT   (*(volatile uint32_t *)(SYSCTL_BASE + 0x1014))
#define SYSCTL_CLKCFG       (*(volatile uint32_t *)(SYSCTL_BASE + 0x1020))
#define SYSCTL_GENCLKEN     (*(volatile uint32_t *)(SYSCTL_BASE + 0x10A0))

/* SYSCTL_GENCLKEN bit definitions for MSPM0G (peripheral clock gates) */
#define GENCLKEN_GPIOA_EN   (1UL << 0)   /* GPIOA clock enable */
#define GENCLKEN_ADC0_EN    (1UL << 10)  /* ADC0 clock enable  */

/* --- GPIOA --------------------------------------------------------------- */
#define GPIOA_BASE          0x400A0000UL
#define GPIOA_DOUT31_0      (*(volatile uint32_t *)(GPIOA_BASE + 0x0080))
#define GPIOA_DSET31_0      (*(volatile uint32_t *)(GPIOA_BASE + 0x0084))
#define GPIOA_DCLR31_0      (*(volatile uint32_t *)(GPIOA_BASE + 0x0088))
#define GPIOA_DIN31_0       (*(volatile uint32_t *)(GPIOA_BASE + 0x008C))
#define GPIOA_DOE31_0       (*(volatile uint32_t *)(GPIOA_BASE + 0x0090))

#define FAN_PIN             (0)  /* PA0 - Fan control output */

/* --- ADC0 (12-bit SAR ADC) ----------------------------------------------- */
#define ADC0_BASE           0x40030000UL
#define ADC0_CTL0           (*(volatile uint32_t *)(ADC0_BASE + 0x0000))
#define ADC0_CTL1           (*(volatile uint32_t *)(ADC0_BASE + 0x0004))
#define ADC0_CTL2           (*(volatile uint32_t *)(ADC0_BASE + 0x0008))
#define ADC0_CTL3           (*(volatile uint32_t *)(ADC0_BASE + 0x000C))
#define ADC0_MEMCTL(n)      (*(volatile uint32_t *)(ADC0_BASE + 0x0010 + (n) * 4))
#define ADC0_MEMRES(n)      (*(volatile uint32_t *)(ADC0_BASE + 0x0060 + (n) * 4))
#define ADC0_STATUS         (*(volatile uint32_t *)(ADC0_BASE + 0x0044))
#define ADC0_IFLG           (*(volatile uint32_t *)(ADC0_BASE + 0x0050))
#define ADC0_CMD            (*(volatile uint32_t *)(ADC0_BASE + 0x006C))

/* ADC0_CTL0 bits */
#define ADC_CTL0_SC         (1UL << 0)   /* Start conversion (auto-clear) */
#define ADC_CTL0_ENC        (1UL << 1)   /* Enable conversions           */
#define ADC_CTL0_PWRDN      (1UL << 28)  /* Power down (0 = powered)     */

/* ADC0_CTL1 bits */
#define ADC_CTL1_AVGD_1X    0UL          /* No averaging */
#define ADC_CTL1_AVGD_4X    (1UL << 4)
#define ADC_CTL1_MODE_SINGLE 0UL         /* Single conversion mode */

/* ADC0_CTL2 bits */
#define ADC_CTL2_DIV_7      0UL          /* ADC clock = bus clk / 1  (DIV=0) */
#define ADC_CTL2_DIV(n)     (((n) & 0x1F) << 0)  /* ADCCLK division (0-31) */
#define ADC_CTL2_REF_VREF   (0UL << 24)  /* Internal VREF (3.3 V) */
#define ADC_CTL2_SAMP(n)    (((n) & 0x7F) << 8)  /* Sample time (ADCCLK cycles) */

/* ADC0_CTL3 bits */
#define ADC_CTL3_TEMP_EN    (1UL << 1)   /* Temperature sensor enable */

/* ADC0_CMD bits */
#define ADC_CMD_SNGSTOP     (1UL << 0)   /* Single conversion command   */

/* ADC0_STATUS bits */
#define ADC_STATUS_MEMRES(n)  (1UL << (n))  /* MEMRES(n) valid          */

/* ADC0_MEMCTL bits */
#define ADC_MEMCTL_CHAN(n)  (((n) & 0x1F) << 0)
#define ADC_MEMCTL_AVG_EN   (1UL << 5)
#define ADC_MEMCTL_WINCOMP  (1UL << 6)

/* Internal temperature sensor channel on MSPM0G */
#define ADC_CHAN_TEMP       12

/* --- SysTick (Cortex-M0+) ------------------------------------------------ */
#define SYSTICK_BASE        0xE000E010UL
#define SYSTICK_CSR         (*(volatile uint32_t *)(SYSTICK_BASE + 0x00))
#define SYSTICK_RVR         (*(volatile uint32_t *)(SYSTICK_BASE + 0x04))
#define SYSTICK_CVR         (*(volatile uint32_t *)(SYSTICK_BASE + 0x08))

#define SYSTICK_CSR_ENABLE  (1UL << 0)
#define SYSTICK_CSR_TICKINT (1UL << 1)
#define SYSTICK_CSR_CLKSOURCE (1UL << 2)  /* 1 = system clock, 0 = ext ref */
#define SYSTICK_CSR_COUNTFLAG (1UL << 16)

/* --------------------------------------------------------------------------
 * Temperature Conversion Constants (approximate; calibrate per device)
 * -------------------------------------------------------------------------- */
/* VREF = 3.3 V, 12-bit ADC => LSB = 3300 / 4096 = 0.80566 mV */
#define VREF_MV             3300.0f
#define ADC_RESOLUTION      4096.0f

/*
 * MSPM0G internal temperature sensor nominal characteristics:
 *   - Slope:  ~1.32 mV/°C  (varies 1.28-1.36)
 *   - Offset: ~650 mV at 0°C (varies ±50 mV)
 *
 * For production, read the factory trim values from the TLV
 * (Tag-Length-Value) structure in flash information memory.
 */
#define TEMP_SLOPE_MV_PER_C 1.32f
#define TEMP_OFFSET_MV_0C   650.0f

/* Temperature thresholds */
#define TEMP_ON_THRESHOLD   30.0f   /* °C - fan turns ON  above this */
#define TEMP_OFF_THRESHOLD  25.0f   /* °C - fan turns OFF below this */

/* --------------------------------------------------------------------------
 * Global State
 * -------------------------------------------------------------------------- */
static volatile uint32_t g_systick_ms = 0;  /* Up-timer in milliseconds */
static volatile uint8_t  g_fan_state = 0;   /* 0=OFF, 1=ON */

/* --------------------------------------------------------------------------
 * SysTick Handler
 * -------------------------------------------------------------------------- */
void SysTick_Handler(void)
{
    g_systick_ms++;
}

/* --------------------------------------------------------------------------
 * Simple Millisecond Delay (blocking, based on SysTick)
 * -------------------------------------------------------------------------- */
static void delay_ms(uint32_t ms)
{
    uint32_t start = g_systick_ms;
    while ((g_systick_ms - start) < ms) {
        /* Spin */
    }
}

/* --------------------------------------------------------------------------
 * Initialize SysTick for 1 ms period
 *
 * Assumes system clock = 32 MHz (MCLK). If using a different MCLK,
 * adjust SYSTICK_RVR accordingly.
 *
 * RVR = (MCLK / 1000) - 1  = 31999 for 32 MHz
 * -------------------------------------------------------------------------- */
static void systick_init(uint32_t mclk_hz)
{
    SYSTICK_RVR = (mclk_hz / 1000U) - 1U;
    SYSTICK_CVR = 0;                       /* Clear current value */
    SYSTICK_CSR = SYSTICK_CSR_ENABLE
                | SYSTICK_CSR_TICKINT
                | SYSTICK_CSR_CLKSOURCE;   /* Use system clock */
}

/* --------------------------------------------------------------------------
 * Initialize GPIOA: configure PA0 as push-pull output for fan control
 * -------------------------------------------------------------------------- */
static void gpio_init(void)
{
    /* Enable GPIOA clock in SYSCTL */
    uint32_t tmp = SYSCTL_GENCLKEN;
    tmp |= GENCLKEN_GPIOA_EN;
    SYSCTL_GENCLKEN = tmp;

    /* Set PA0 as output (DOE = 1), initially LOW (fan OFF) */
    GPIOA_DCLR31_0 = (1UL << FAN_PIN);  /* Ensure output is LOW */
    GPIOA_DOE31_0 |= (1UL << FAN_PIN);  /* Enable output driver  */
}

/* --------------------------------------------------------------------------
 * Initialize ADC0 for internal temperature sensor reading
 *
 * Configures:
 *   - 12-bit single-ended mode, internal VREF (3.3 V)
 *   - Temperature sensor enabled
 *   - ADC clock = bus clock / 8 for reliable sampling
 *   - 16-cycle sampling time
 *   - Memory slot 0 reads channel 12 (temperature sensor)
 * -------------------------------------------------------------------------- */
static void adc_init(void)
{
    /* Enable ADC0 clock in SYSCTL */
    uint32_t tmp = SYSCTL_GENCLKEN;
    tmp |= GENCLKEN_ADC0_EN;
    SYSCTL_GENCLKEN = tmp;

    /* --- Power up and configure ADC --- */

    /* CTL0: leave powered (PWRDN=0), enable conversion */
    ADC0_CTL0 = ADC_CTL0_ENC;

    /* CTL1: single conversion, no averaging initially */
    ADC0_CTL1 = ADC_CTL1_MODE_SINGLE | ADC_CTL1_AVGD_1X;

    /* CTL2: ADC clock divide by 8, internal VREF, sample time = 16 ADCCLK */
    ADC0_CTL2 = ADC_CTL2_DIV(7)           /* DIV=7 => /8           */
              | ADC_CTL2_REF_VREF         /* Internal VREF (3.3V)  */
              | ADC_CTL2_SAMP(15);        /* Sample time = 16 clks */

    /* CTL3: enable internal temperature sensor */
    ADC0_CTL3 = ADC_CTL3_TEMP_EN;

    /*
     * MEMCTL0: select temperature sensor channel (CH12),
     * no averaging, no window comparator.
     */
    ADC0_MEMCTL(0) = ADC_MEMCTL_CHAN(ADC_CHAN_TEMP);
}

/* --------------------------------------------------------------------------
 * Trigger a single ADC conversion and return the raw 12-bit result
 * -------------------------------------------------------------------------- */
static uint16_t adc_read_single(void)
{
    /*
     * ENC must be set for conversions to run.
     * Trigger a single conversion on memory slot 0.
     */
    ADC0_CMD = ADC_CMD_SNGSTOP;  /* Issue single-conversion command */

    /* Wait for MEMRES(0) to be valid */
    while (!(ADC0_STATUS & ADC_STATUS_MEMRES(0))) {
        /* Spin */
    }

    /* Read and return the 12-bit result (lower 12 bits) */
    return (uint16_t)(ADC0_MEMRES(0) & 0x0FFFUL);
}

/* --------------------------------------------------------------------------
 * Convert raw ADC reading to temperature in degrees Celsius
 * -------------------------------------------------------------------------- */
static float adc_to_temperature_c(uint16_t adc_raw)
{
    /* Convert ADC code to voltage in mV */
    float voltage_mv = ((float)adc_raw * VREF_MV) / ADC_RESOLUTION;

    /* Convert voltage to temperature */
    float temperature_c = (voltage_mv - TEMP_OFFSET_MV_0C) / TEMP_SLOPE_MV_PER_C;

    return temperature_c;
}

/* --------------------------------------------------------------------------
 * Set fan state: 1 = ON (PA0 HIGH), 0 = OFF (PA0 LOW)
 * -------------------------------------------------------------------------- */
static void fan_set(uint8_t on)
{
    if (on) {
        GPIOA_DSET31_0 = (1UL << FAN_PIN);  /* Set PA0 HIGH */
    } else {
        GPIOA_DCLR31_0 = (1UL << FAN_PIN);  /* Set PA0 LOW  */
    }
    g_fan_state = on;
}

/* --------------------------------------------------------------------------
 * Main Entry Point
 * -------------------------------------------------------------------------- */
int main(void)
{
    float temperature;
    uint16_t adc_raw;

    /*
     * Assume system is already running from internal oscillator at 32 MHz
     * after reset boot code. No external crystal configuration needed
     * for this application.
     *
     * MCLK = 32 MHz -> SysTick reload = 31999 for 1 ms ticks.
     */
    systick_init(32000000UL);

    /* Initialize peripherals */
    gpio_init();
    adc_init();

    /* Initial fan state: OFF */
    fan_set(0);

    /* Allow the ADC, temperature sensor, and references to settle */
    delay_ms(100);

    /* --- Main control loop --- */
    while (1) {
        /* Read temperature sensor */
        adc_raw = adc_read_single();
        temperature = adc_to_temperature_c(adc_raw);

        /* Hysteresis-based fan control */
        if (temperature > TEMP_ON_THRESHOLD) {
            /* Above 30°C -> turn fan ON */
            if (g_fan_state == 0) {
                fan_set(1);
            }
        } else if (temperature < TEMP_OFF_THRESHOLD) {
            /* Below 25°C -> turn fan OFF */
            if (g_fan_state == 1) {
                fan_set(0);
            }
        }
        /* Between 25°C and 30°C: hold current state (hysteresis) */

        /* Poll every 500 ms */
        delay_ms(500);
    }

    /* Unreachable */
    return 0;
}

/* --------------------------------------------------------------------------
 * Minimal Default Handlers for unexpected interrupts
 * -------------------------------------------------------------------------- */
void Default_Handler(void)
{
    while (1) { /* Trap */ }
}

void NMI_Handler(void)           __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)           __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)        __attribute__((weak, alias("Default_Handler")));
