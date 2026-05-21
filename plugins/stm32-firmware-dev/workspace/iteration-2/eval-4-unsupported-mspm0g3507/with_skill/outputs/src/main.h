/**
 * main.h — MSPM0G3507 Temperature-Controlled Fan
 *
 * CHIP COMPATIBILITY (Phase -1): MANUAL ONLY quadrant
 *   - Docs MCP:  NO — only STM32F103C8 indexed; MSPM0G3507 is a TI chip
 *   - Sim MCP:   NO — QEMU only supports STM32F1xx/F4xx
 *   - All register values below come from web research (TI SLAU846 TRM,
 *     SLASEX6 datasheet, TI E2E forum), NOT from MCP-verified sources.
 *   - This code CANNOT be tested until real MSPM0G3507 hardware is available.
 *
 * Hardware: MSPM0G3507 (TI, Arm Cortex-M0+, 80 MHz, 128KB Flash, 32KB SRAM)
 * Clock:    SYSOSC 32 MHz (default, no PLL needed for this application)
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

/*===========================================================================
 * Clock configuration
 *===========================================================================*/
#define SYSOSC_FREQ      32000000UL   /* Internal oscillator, default 32 MHz */
#define MCLK_FREQ        SYSOSC_FREQ  /* MCLK = SYSOSC (no PLL) */

/*===========================================================================
 * GPIO0 (GPIOA) — Fan control on PA0
 *===========================================================================*/
#define GPIOA_BASE       0x400A0000UL
#define GPIOA_PWREN      (*(volatile uint32_t *)(GPIOA_BASE + 0x800))
#define GPIOA_DOE31_0    (*(volatile uint32_t *)(GPIOA_BASE + 0x12C0))
#define GPIOA_DOUTSET    (*(volatile uint32_t *)(GPIOA_BASE + 0x1290))
#define GPIOA_DOUTCLR    (*(volatile uint32_t *)(GPIOA_BASE + 0x12A0))
#define GPIOA_DOUTTGL    (*(volatile uint32_t *)(GPIOA_BASE + 0x12B0))
#define GPIOA_DIN31_0    (*(volatile uint32_t *)(GPIOA_BASE + 0x1380))

#define FAN_PIN           0           /* PA0 controls the fan (active high) */

/* PWREN register: KEY[31:24]=0x26, ENABLE[0]=1 */
#define PWREN_KEY_ENABLE  0x26000001UL

/*===========================================================================
 * ADC0 — Internal temperature sensor on channel 11
 *===========================================================================*/
#define ADC0_BASE         0x40000000UL
#define ADC0_PWREN        (*(volatile uint32_t *)(ADC0_BASE + 0x800))
#define ADC0_CLKCFG       (*(volatile uint32_t *)(ADC0_BASE + 0x808))
#define ADC0_CTL0         (*(volatile uint32_t *)(ADC0_BASE + 0x1100))
#define ADC0_CTL1         (*(volatile uint32_t *)(ADC0_BASE + 0x1104))
#define ADC0_CTL2         (*(volatile uint32_t *)(ADC0_BASE + 0x1108))
#define ADC0_SCOMP0       (*(volatile uint32_t *)(ADC0_BASE + 0x1114))
#define ADC0_MEMCTL0      (*(volatile uint32_t *)(ADC0_BASE + 0x1180))
#define ADC0_STATUS       (*(volatile uint32_t *)(ADC0_BASE + 0x1340))

/*
 * ADC0 MEMRES[0] aliased region (MCLK-speed access, recommended by TI).
 * Physical base: 0x40556000.  Each MEMRES[y] is at base + y*4.
 * Source: MSPM0G350x datasheet peripheral file map (SLASF88A).
 */
#define ADC0_MEMRES_ALIAS_BASE  0x40556000UL
#define ADC0_MEMRES0            (*(volatile uint32_t *)(ADC0_MEMRES_ALIAS_BASE + 0))

/* ADC CTL0 bitfields (SLAU846 TRM, Table 17-17) */
#define ADC_CTL0_SCLKDIV_Pos   24
#define ADC_CTL0_SCLKDIV_Msk   (0x7UL << ADC_CTL0_SCLKDIV_Pos)
#define ADC_CTL0_SCLKDIV_DIV8  (0x3UL << ADC_CTL0_SCLKDIV_Pos)  /* /8 */
#define ADC_CTL0_PWRDN_Pos     16
#define ADC_CTL0_PWRDN_AUTO    (0x0UL << ADC_CTL0_PWRDN_Pos)    /* auto pwr-down */

/* ADC CTL1 bitfields (SLAU846 TRM, Table 17-18) */
#define ADC_CTL1_SC_Pos        0
#define ADC_CTL1_ENC_Pos       1
#define ADC_CTL1_CONSEQ_Pos    2
#define ADC_CTL1_CONSEQ_SINGLE (0x0UL << ADC_CTL1_CONSEQ_Pos)   /* single conversion */
#define ADC_CTL1_TRIGSRC_Pos   4
#define ADC_CTL1_TRIGSRC_SW    (0x0UL << ADC_CTL1_TRIGSRC_Pos)  /* software trigger */
#define ADC_CTL1_SAMPMODE_Pos  6
#define ADC_CTL1_SAMPMODE_AUTO (0x0UL << ADC_CTL1_SAMPMODE_Pos) /* auto sample timer */

/* ADC CTL2 bitfields (SLAU846 TRM, Table 17-19) */
#define ADC_CTL2_RES_Pos       0
#define ADC_CTL2_RES_12BIT     (0x0UL << ADC_CTL2_RES_Pos)      /* 12-bit resolution */
#define ADC_CTL2_STARTADD_Pos  2
#define ADC_CTL2_ENDADD_Pos    6

/* ADC MEMCTL bitfields (SLAU846 TRM, Table 17-23) */
#define ADC_MEMCTL_CHANSEL_Pos 0
#define ADC_MEMCTL_VRSEL_Pos   8
#define ADC_MEMCTL_VRSEL_VDD   (0x0UL << ADC_MEMCTL_VRSEL_Pos)  /* VDD=VDDA reference */
#define ADC_MEMCTL_STIME_Pos   12
#define ADC_MEMCTL_STIME_SCOMP0 (0x0UL << ADC_MEMCTL_STIME_Pos) /* use SCOMP0 */

/* ADC STATUS bitfields (SLAU846 TRM) */
#define ADC_STATUS_BUSY_Pos    0
#define ADC_STATUS_BUSY_Msk    (0x1UL << ADC_STATUS_BUSY_Pos)

/* Temperature sensor channel */
#define ADC_CHANNEL_TEMPSENSE  11

/*===========================================================================
 * SYSCTL / Factory Constants
 *===========================================================================*/
#define SYSCTL_BASE        0x400AF000UL

/*
 * TEMP_SENSE0.DATA: Factory calibration constant at 30°C (TSTRIM).
 * Address: 0x41C4003C (FACTORY region base 0x41C40000 + offset 0x3C).
 * Contains the 12-bit ADC code measured at VDDA=3.3V, T=30°C.
 * Source: SLAU846 TRM, Factory Region table.
 */
#define FACTORY_TEMP_SENSE0_ADDR  0x41C4003CUL
#define FACTORY_TEMP_SENSE0       (*(volatile uint32_t *)(FACTORY_TEMP_SENSE0_ADDR))

/*===========================================================================
 * Temperature calculation parameters
 *===========================================================================*/
#define VDDA_MV            3300        /* Reference voltage in mV */
#define ADC_MAX_CODE       4096        /* 12-bit ADC */
#define TSTRIM_C           30          /* Factory trim temperature (°C) */
#define TSC_MV_PER_C_NEG   1800        /* Temp coefficient: -1.8 mV/°C, stored as 1.800 */
                                       /* (positive for integer math convenience) */
#define TEMP_ON_THRESHOLD  30          /* Fan ON  when T > 30°C (in °C) */
#define TEMP_OFF_THRESHOLD 25          /* Fan OFF when T < 25°C (in °C) */

/*===========================================================================
 * Delay
 *===========================================================================*/
/* Number of SysTick reload cycles for 1 ms at 32 MHz */
#define SYSTICK_LOAD_1MS   (MCLK_FREQ / 1000 - 1)  /* 31999 */

/* Simple delay macros */
void delay_ms(uint32_t ms);
void delay_init(void);

#endif /* MAIN_H */
