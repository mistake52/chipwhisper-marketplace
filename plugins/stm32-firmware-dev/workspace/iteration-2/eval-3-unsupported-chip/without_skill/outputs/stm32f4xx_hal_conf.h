/**
 * @file    stm32f4xx_hal_conf.h
 * @brief   HAL configuration file for STM32F407VGT6.
 *
 * This file enables the required HAL modules and configures the HAL
 * tick frequency for this project.
 */

#ifndef STM32F4XX_HAL_CONF_H
#define STM32F4XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * HAL Module Activation
 * -------------------------------------------------------------------------- */
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CAN_MODULE_ENABLED
#define HAL_CAN_LEGACY_MODULE_ENABLED
#define HAL_CRC_MODULE_ENABLED
#define HAL_CEC_MODULE_ENABLED
#define HAL_CRYPT_MODULE_ENABLED
#define HAL_DAC_MODULE_ENABLED
#define HAL_DCMI_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_DMA2D_MODULE_ENABLED
#define HAL_ETH_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_NAND_MODULE_ENABLED
#define HAL_NOR_MODULE_ENABLED
#define HAL_PCCARD_MODULE_ENABLED
#define HAL_SRAM_MODULE_ENABLED
#define HAL_SDRAM_MODULE_ENABLED
#define HAL_HASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_I2S_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_LTDC_MODULE_ENABLED
#define HAL_DSI_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_QSPI_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_RNG_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#define HAL_SAI_MODULE_ENABLED
#define HAL_SD_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_USART_MODULE_ENABLED
#define HAL_IRDA_MODULE_ENABLED
#define HAL_SMARTCARD_MODULE_ENABLED
#define HAL_WWDG_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HAL_HCD_MODULE_ENABLED
#define HAL_FMPI2C_MODULE_ENABLED
#define HAL_SPDIFRX_MODULE_ENABLED
#define HAL_DFSDM_MODULE_ENABLED
#define HAL_LPTIM_MODULE_ENABLED
#define HAL_MMC_MODULE_ENABLED

/* --------------------------------------------------------------------------
 * HAL Tick Configuration
 * -------------------------------------------------------------------------- */
#define TICK_INT_PRIORITY       0x0FU  /* Lowest priority */
#define USE_RTOS                0U
#define PREFETCH_ENABLE         1U
#define INSTRUCTION_CACHE_ENABLE 1U
#define DATA_CACHE_ENABLE       1U
#define USE_SPI_CRC             0U

/* --------------------------------------------------------------------------
 * Oscillator Values (in Hz)
 * -------------------------------------------------------------------------- */
#define HSE_VALUE               8000000U    /* 8 MHz from ST-LINK MCO */
#define HSI_VALUE               16000000U   /* 16 MHz HSI (typical) */
#define LSE_VALUE               32768U      /* 32.768 kHz */
#define LSI_VALUE               32000U      /* 32 kHz LSI (typical) */
#define EXTERNAL_CLOCK_VALUE    12288000U   /* Not used */

/* --------------------------------------------------------------------------
 * HAL Assert Configuration
 * -------------------------------------------------------------------------- */
#ifdef USE_FULL_ASSERT
#define assert_param(expr)      ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr)      ((void)0U)
#endif

/* --------------------------------------------------------------------------
 * Ethernet configuration (not used, but defined for compatibility)
 * -------------------------------------------------------------------------- */
#define ETH_TX_DESC_CNT         4U
#define ETH_RX_DESC_CNT         4U
#define ETH_DMA_TX_DESC_CNT     4U
#define ETH_DMA_RX_DESC_CNT     4U
#define ETH_MAC_ADDR0           ((uint8_t)0x02)
#define ETH_MAC_ADDR1           ((uint8_t)0x00)
#define ETH_MAC_ADDR2           ((uint8_t)0x00)
#define ETH_MAC_ADDR3           ((uint8_t)0x00)
#define ETH_MAC_ADDR4           ((uint8_t)0x00)
#define ETH_MAC_ADDR5           ((uint8_t)0x00)

#ifdef __cplusplus
}
#endif

#endif /* STM32F4XX_HAL_CONF_H */
