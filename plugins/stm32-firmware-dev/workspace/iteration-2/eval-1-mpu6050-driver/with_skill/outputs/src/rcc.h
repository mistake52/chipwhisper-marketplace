/**
 * rcc.h — System clock configuration for STM32F103C8T6
 *
 * MCP-verified register layout (RM0008 Rev 21):
 *   RCC_CR:     0x40021000+0x00, reset=0x00000083
 *     HSEON=16, HSERDY=17, HSEBYP=18, PLLON=24, PLLRDY=25
 *   RCC_CFGR:   0x40021000+0x04, reset=0x00000000
 *     SW=[0:1], HPRE=[4:7], PPRE1=[8:10], PPRE2=[11:13],
 *     PLLSRC=16, PLLXTPRE=17, PLLMUL=[18:21]
 *   RCC_APB2ENR: 0x40021000+0x18 (IOPAEN=2, IOPBEN=3, USART1EN=14, AFIOEN=0)
 *   RCC_APB1ENR: 0x40021000+0x1C (I2C1EN=21)
 */

#ifndef RCC_H
#define RCC_H

#include <stdint.h>

/* --- RCC Register Definitions (MCP-verified) --- */
#define RCC_BASE        0x40021000UL
#define RCC_CR          (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_CFGR        (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_CIR         (*(volatile uint32_t*)(RCC_BASE + 0x08))
#define RCC_APB2RSTR    (*(volatile uint32_t*)(RCC_BASE + 0x0C))
#define RCC_APB1RSTR    (*(volatile uint32_t*)(RCC_BASE + 0x10))
#define RCC_AHBENR      (*(volatile uint32_t*)(RCC_BASE + 0x14))
#define RCC_APB2ENR     (*(volatile uint32_t*)(RCC_BASE + 0x18))
#define RCC_APB1ENR     (*(volatile uint32_t*)(RCC_BASE + 0x1C))
#define RCC_BDCR        (*(volatile uint32_t*)(RCC_BASE + 0x20))
#define RCC_CSR         (*(volatile uint32_t*)(RCC_BASE + 0x24))

/* RCC_CR bit definitions */
#define RCC_CR_HSION    (1UL << 0)
#define RCC_CR_HSIRDY   (1UL << 1)
#define RCC_CR_HSEON    (1UL << 16)
#define RCC_CR_HSERDY   (1UL << 17)
#define RCC_CR_HSEBYP   (1UL << 18)
#define RCC_CR_PLLON    (1UL << 24)
#define RCC_CR_PLLRDY   (1UL << 25)

/* RCC_CFGR bit definitions */
#define RCC_CFGR_SW_HSI         0x00
#define RCC_CFGR_SW_HSE         0x01
#define RCC_CFGR_SW_PLL         0x02
#define RCC_CFGR_SWS_MASK       0x0C
#define RCC_CFGR_SWS_HSI        (0x00 << 2)
#define RCC_CFGR_SWS_HSE        (0x01 << 2)
#define RCC_CFGR_SWS_PLL        (0x02 << 2)
#define RCC_CFGR_HPRE_DIV1      0x00000000
#define RCC_CFGR_PPRE1_DIV2     0x00000400  /* APB1 /2 → 36MHz */
#define RCC_CFGR_PPRE2_DIV1     0x00000000  /* APB2 /1 → 72MHz */
#define RCC_CFGR_PLLSRC_HSE     (1UL << 16) /* HSE as PLL source */
#define RCC_CFGR_PLLXTPRE_DIV1  0x00000000  /* HSE/1 */
#define RCC_CFGR_PLLMUL_9       (0x07 << 18) /* x9 = 72MHz */

/* RCC_APB2ENR bit definitions (MCP-verified) */
#define RCC_APB2ENR_AFIOEN     (1UL << 0)
#define RCC_APB2ENR_IOPAEN     (1UL << 2)
#define RCC_APB2ENR_IOPBEN     (1UL << 3)
#define RCC_APB2ENR_USART1EN   (1UL << 14)

/* RCC_APB1ENR bit definitions (MCP-verified) */
#define RCC_APB1ENR_I2C1EN     (1UL << 21)

/* --- FLASH Register Definitions --- */
#define FLASH_BASE      0x40022000UL
#define FLASH_ACR       (*(volatile uint32_t*)(FLASH_BASE + 0x00))

/* FLASH_ACR bit definitions */
#define FLASH_ACR_LATENCY_0   0x00000000
#define FLASH_ACR_LATENCY_1   (1UL << 0)  /* 24-48MHz: 1 wait state */
#define FLASH_ACR_LATENCY_2   (2UL << 0)  /* 48-72MHz: 2 wait states */

void rcc_init(void);

#endif /* RCC_H */
