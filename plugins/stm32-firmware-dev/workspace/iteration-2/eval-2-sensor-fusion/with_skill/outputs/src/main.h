/**
 * main.h — System clock macros and global configuration
 *
 * MCP-Verified Register Values (Phase 0):
 *   Chip: STM32F103C8, RM0008 Rev 21
 *   HSE: 8MHz crystal
 *   PLL: HSE × 9 = 72MHz
 *   APB1: 36MHz (PPRE1=4 → divide by 2)
 *   APB2: 72MHz (PPRE2=0 → no divide)
 *   AHB:  72MHz (HPRE=0 → no divide)
 *
 * Simulation (QEMU stm32vldiscovery = F100):
 *   When SIMULATION is defined, skip PLL and run from HSI 8MHz.
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

/* Clock frequencies — hardware mode */
#define HSE_VALUE       8000000UL
#define SYSCLK_FREQ     72000000UL
#define AHB_FREQ        SYSCLK_FREQ
#define APB1_FREQ       36000000UL
#define APB2_FREQ       72000000UL

/* Simulation override */
#ifdef SIMULATION
#undef  SYSCLK_FREQ
#define SYSCLK_FREQ     8000000UL
#undef  AHB_FREQ
#define AHB_FREQ        SYSCLK_FREQ
#undef  APB1_FREQ
#define APB1_FREQ       SYSCLK_FREQ
#undef  APB2_FREQ
#define APB2_FREQ       SYSCLK_FREQ
#endif

/* I2C addresses (7-bit, left-shifted for STM32 DR write) */
#define MPU6050_ADDR    0xD0   /* AD0=GND → 7-bit 0x68, write = 0xD0 */
#define HMC5883L_ADDR   0x3C   /* 7-bit 0x1E, write = 0x3C */

/* OLED dimensions */
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

/* Sensor fusion sample rate */
#define SENSOR_HZ       100
#define SENSOR_DT_MS    (1000 / SENSOR_HZ)

#endif /* MAIN_H */
