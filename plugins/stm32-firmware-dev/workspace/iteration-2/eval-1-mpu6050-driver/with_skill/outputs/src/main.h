/**
 * main.h — MPU6050 Driver for STM32F103C8T6 (bare-metal, no HAL)
 *
 * Phase 0 Documentation Research (MCP-verified, RM0008 Rev 21):
 *   - I2C1 base: 0x40005400 (APB1)
 *   - USART1 base: 0x40013800 (APB2)
 *   - GPIOA base: 0x40010800 (APB2), GPIOB base: 0x40010C00 (APB2)
 *   - RCC base: 0x40021000 (AHB), FLASH base: 0x40022000 (AHB)
 *   - Pin assignments: PB6=I2C1_SCL(AF_OD), PB7=I2C1_SDA(AF_OD), PA9=USART1_TX(AF_PP)
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

/* Clock frequencies */
#define HSE_VALUE       8000000UL   /* External 8MHz crystal */
#define HSI_VALUE       8000000UL   /* Internal 8MHz RC */
#define SYSCLK_FREQ     72000000UL  /* 72MHz: HSE PLL x9 */
#define APB2_FREQ       72000000UL  /* APB2 /1 */
#define APB1_FREQ       36000000UL  /* APB1 /2 (max 36MHz) */

/* SysTick */
#define SYSTICK_HZ      1000UL      /* 1ms ticks */

/* UART settings */
#define UART_BAUD       115200UL

/* I2C settings */
#define I2C_SPEED_HZ    100000UL    /* 100kHz standard mode */

#endif /* MAIN_H */
