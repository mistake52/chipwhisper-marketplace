/**
 * main.h — Project-wide definitions for STM32F407VGT6 LM75 I2C Temperature Project
 *
 * Register values from general knowledge — NOT MCP-verified.
 * (STM32F4xx not in embedded-docs knowledge base; only F103 is covered.)
 *
 * Target board: STM32F407VGT6 Discovery (STM32F407G-DISC1)
 * QEMU machine: netduinoplus2
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

/* ================================================================
 * Clock frequencies (Hz)
 * HSE = 8MHz onboard crystal (hardware)
 * HSI = 16MHz internal RC (simulation)
 * ================================================================ */
#ifndef HSE_VALUE
#define HSE_VALUE       8000000UL
#endif

#ifdef SIMULATION
/* Simulation: HSI = 16MHz, no prescalers, no PLL */
#define SYSCLK_FREQ      16000000UL
#define HCLK_FREQ        16000000UL
#define PCLK1_FREQ       16000000UL
#define PCLK2_FREQ       16000000UL
#else
/* Hardware: HSE(8MHz) -> PLL -> 168MHz */
#define SYSCLK_FREQ     168000000UL
#define HCLK_FREQ       168000000UL
#define PCLK1_FREQ       42000000UL   /* APB1: divided by 4 from HCLK  */
#define PCLK2_FREQ       84000000UL   /* APB2: divided by 2 from HCLK  */
#endif

/* ================================================================
 * Peripheral Base Addresses (STM32F40x memory map)
 * ================================================================ */
#define FLASH_BASE      0x08000000UL
#define SRAM_BASE       0x20000000UL
#define PERIPH_BASE     0x40000000UL

/* AHB1 peripherals */
#define GPIOA_BASE      (PERIPH_BASE + 0x20000UL)
#define GPIOB_BASE      (PERIPH_BASE + 0x20400UL)
#define GPIOC_BASE      (PERIPH_BASE + 0x20800UL)

/* APB1 peripherals */
#define I2C1_BASE       (PERIPH_BASE + 0x05400UL)

/* APB2 peripherals */
#define SPI1_BASE       (PERIPH_BASE + 0x13000UL)

/* AHB1 peripherals */
#define RCC_BASE        (PERIPH_BASE + 0x23800UL)

/* Private peripherals */
#define FLASH_IF_BASE   0x40023C00UL
#define SYSTICK_BASE    0xE000E010UL

/* ================================================================
 * I2C1 configuration
 * Bus: APB1 @ 42MHz
 * Speed: Standard mode (100kHz)
 * ================================================================ */
#define I2C1_SCL_PIN    6
#define I2C1_SDA_PIN    7
#define I2C1_AF          4   /* Alternate function 4 for I2C1-3 on F4 */

/* I2C timing: CCR = PCLK1 / (2 * 100kHz), TRISE = (1000ns / T_PCLK1) + 1 */
#ifdef SIMULATION
#define I2C1_CCR_VALUE  80    /* 16MHz / 200kHz = 80 */
#define I2C1_TRISE_VALUE 17   /* 1000ns / 62.5ns + 1 = 17 */
#else
#define I2C1_CCR_VALUE  210   /* 42MHz / 200kHz = 210 */
#define I2C1_TRISE_VALUE 43   /* 1000ns / 23.8ns + 1 = 43 */
#endif

/* ================================================================
 * LM75 I2C temperature sensor
 * 7-bit address: 0x48 (A2=A1=A0=0)
 * 8-bit write: 0x90, 8-bit read: 0x91
 * ================================================================ */
#define LM75_ADDR       0x48
#define LM75_ADDR_W     ((LM75_ADDR << 1) | 0)
#define LM75_ADDR_R     ((LM75_ADDR << 1) | 1)

/* LM75 registers */
#define LM75_REG_TEMP   0x00
#define LM75_REG_CONF   0x01
#define LM75_REG_THYST  0x02
#define LM75_REG_TOS    0x03

/* ================================================================
 * ILI9341 LCD (SPI mode) pin assignments
 * ================================================================ */
#define LCD_SPI         SPI1
#define LCD_SPI_BASE    SPI1_BASE

#define LCD_RST_PORT    GPIOC
#define LCD_RST_PIN     6
#define LCD_DC_PORT     GPIOC
#define LCD_DC_PIN      5
#define LCD_CS_PORT     GPIOC
#define LCD_CS_PIN      4

/* ================================================================
 * Function declarations
 * ================================================================ */
void rcc_init(void);
void systick_init(void);
void delay_ms(uint32_t ms);
uint32_t millis(void);
void i2c_init(void);
int  lm75_read_temp(int16_t *temp_raw);
void lcd_init(void);
void lcd_clear(uint16_t color);
void lcd_draw_string(int x, int y, const char *str, uint16_t fg, uint16_t bg);

#endif /* MAIN_H */
