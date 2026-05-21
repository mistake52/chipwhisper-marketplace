/**
 * rcc.c — System clock init: HSE 8MHz → PLL x9 → 72MHz
 *
 * MCP-verified register values (RM0008 Rev 21, sec 7.3):
 *   Clock tree: HSE(8MHz) → PLL x9 → 72MHz SYSCLK
 *   AHB /1 = 72MHz, APB1 /2 = 36MHz, APB2 /1 = 72MHz
 *   FLASH_ACR.LATENCY = 2 (for 48-72MHz range)
 */

#include "rcc.h"

void rcc_init(void)
{
    uint32_t timeout;

    /* 1. Set FLASH latency: 2 wait states for 48-72MHz */
    FLASH_ACR = FLASH_ACR_LATENCY_2;

    /* 2. Enable HSE (external 8MHz crystal) */
    RCC_CR |= RCC_CR_HSEON;

    /* 3. Wait for HSE to become ready */
    timeout = 1000000;
    while (!(RCC_CR & RCC_CR_HSERDY)) {
        if (--timeout == 0) break;
    }

    /* 4. Configure PLL: HSE/1 = 8MHz → PLL x9 = 72MHz */
    RCC_CFGR = 0
        | RCC_CFGR_PLLSRC_HSE          /* PLL source = HSE */
        | RCC_CFGR_PLLXTPRE_DIV1       /* HSE/1 */
        | RCC_CFGR_PLLMUL_9            /* x9 = 72MHz */
        | RCC_CFGR_HPRE_DIV1           /* AHB /1 */
        | RCC_CFGR_PPRE1_DIV2          /* APB1 /2 = 36MHz (max) */
        | RCC_CFGR_PPRE2_DIV1;         /* APB2 /1 = 72MHz */

    /* 5. Enable PLL */
    RCC_CR |= RCC_CR_PLLON;

    /* 6. Wait for PLL to become ready */
    timeout = 1000000;
    while (!(RCC_CR & RCC_CR_PLLRDY)) {
        if (--timeout == 0) break;
    }

    /* 7. Switch system clock to PLL */
    RCC_CFGR &= ~0x03;         /* Clear SW[1:0] */
    RCC_CFGR |= RCC_CFGR_SW_PLL;

    /* 8. Wait for PLL to become the system clock */
    timeout = 1000000;
    while ((RCC_CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL) {
        if (--timeout == 0) break;
    }
}
