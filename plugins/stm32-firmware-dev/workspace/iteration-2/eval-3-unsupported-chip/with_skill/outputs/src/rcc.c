/**
 * rcc.c — Clock configuration for STM32F407VGT6 (168MHz)
 *
 * Register values from general knowledge — NOT MCP-verified.
 * STM32F4xx maps RCC to 0x40023800 (F1 uses 0x40021000).
 * GPIO enable is on AHB1ENR on F4 (not APB2ENR as on F1).
 *
 * Clock path: HSE(8MHz) → /PLLM(8) → ×PLLN(336) → /PLLP(2) → 168MHz SYSCLK
 * AHB  prescaler: /1  → HCLK  = 168MHz
 * APB1 prescaler: /4  → PCLK1 =  42MHz (max 42MHz for F40x APB1)
 * APB2 prescaler: /2  → PCLK2 =  84MHz (max 84MHz for F40x APB2)
 */

#include "main.h"

/* ================================================================
 * RCC Register Definitions (STM32F40x: base 0x40023800)
 * ================================================================ */
#define RCC_CR          (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_PLLCFGR     (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_CFGR        (*(volatile uint32_t*)(RCC_BASE + 0x08))
#define RCC_AHB1ENR     (*(volatile uint32_t*)(RCC_BASE + 0x30))
#define RCC_APB1ENR     (*(volatile uint32_t*)(RCC_BASE + 0x40))
#define RCC_APB2ENR     (*(volatile uint32_t*)(RCC_BASE + 0x44))

/* RCC_CR bits */
#define RCC_CR_HSION     (1U << 0)
#define RCC_CR_HSIRDY    (1U << 1)
#define RCC_CR_HSEON     (1U << 16)
#define RCC_CR_HSERDY    (1U << 17)
#define RCC_CR_HSEBYP    (1U << 18)
#define RCC_CR_PLLON     (1U << 24)
#define RCC_CR_PLLRDY    (1U << 25)

/* RCC_PLLCFGR bits (F4-specific, different from F1's PLLMUL) */
#define RCC_PLLCFGR_PLLM_Pos   0
#define RCC_PLLCFGR_PLLM_Msk   (0x3FU << RCC_PLLCFGR_PLLM_Pos)
#define RCC_PLLCFGR_PLLN_Pos   6
#define RCC_PLLCFGR_PLLN_Msk   (0x1FFU << RCC_PLLCFGR_PLLN_Pos)
#define RCC_PLLCFGR_PLLP_Pos   16
#define RCC_PLLCFGR_PLLP_Msk   (0x3U << RCC_PLLCFGR_PLLP_Pos)
/* PLLP values: 00→2, 01→4, 10→6, 11→8 */
#define RCC_PLLCFGR_PLLP_DIV2   0x00000000U
#define RCC_PLLCFGR_PLLP_DIV4   (1U << 16)
#define RCC_PLLCFGR_PLLSRC_HSE  (1U << 22)

/* RCC_CFGR bits */
#define RCC_CFGR_SW_Pos         0
#define RCC_CFGR_SW_Msk         (0x3U << RCC_CFGR_SW_Pos)
#define RCC_CFGR_SW_HSI         0x00000000U
#define RCC_CFGR_SW_HSE         0x00000001U
#define RCC_CFGR_SW_PLL         0x00000002U
#define RCC_CFGR_SWS_Pos        2
#define RCC_CFGR_SWS_Msk        (0x3U << RCC_CFGR_SWS_Pos)
#define RCC_CFGR_SWS_PLL        0x00000008U
#define RCC_CFGR_HPRE_Pos       4
#define RCC_CFGR_HPRE_DIV1      0x00000000U   /* AHB = SYSCLK / 1 */
#define RCC_CFGR_PPRE1_Pos      10
#define RCC_CFGR_PPRE1_DIV4     0x00001400U   /* APB1 = SYSCLK / 4 (PPRE1=101) */
#define RCC_CFGR_PPRE2_Pos      13
#define RCC_CFGR_PPRE2_DIV2     0x00008000U   /* APB2 = SYSCLK / 2 (PPRE2=100) */

/* RCC_AHB1ENR bits (F4: GPIOs are on AHB1, not APB2!) */
#define RCC_AHB1ENR_GPIOAEN     (1U << 0)
#define RCC_AHB1ENR_GPIOBEN     (1U << 1)
#define RCC_AHB1ENR_GPIOCEN     (1U << 2)
#define RCC_AHB1ENR_GPIODEN     (1U << 3)
#define RCC_AHB1ENR_GPIOEEN     (1U << 4)
#define RCC_AHB1ENR_GPIOFEN     (1U << 5)

/* RCC_APB1ENR bits */
#define RCC_APB1ENR_TIM2EN      (1U << 0)
#define RCC_APB1ENR_USART2EN    (1U << 17)
#define RCC_APB1ENR_I2C1EN      (1U << 21)

/* RCC_APB2ENR bits */
#define RCC_APB2ENR_USART1EN    (1U << 4)
#define RCC_APB2ENR_SPI1EN      (1U << 12)
#define RCC_APB2ENR_SYSCFGEN    (1U << 14)

/* ================================================================
 * FLASH Interface Register (STM32F40x: base 0x40023C00)
 * ================================================================ */
#define FLASH_ACR       (*(volatile uint32_t*)(FLASH_IF_BASE + 0x00))
/* Wait states for 168MHz: 5 WS (LATENCY=5) */
#define FLASH_ACR_LATENCY_Pos   0
#define FLASH_ACR_LATENCY_5WS   0x00000005U
#define FLASH_ACR_PRFTEN        (1U << 8)
#define FLASH_ACR_ICEN          (1U << 9)
#define FLASH_ACR_DCEN          (1U << 10)

/* ================================================================
 * PWR peripheral (for voltage regulator scaling on F4)
 * ================================================================ */
#define PWR_BASE        0x40007000UL
#define PWR_CR          (*(volatile uint32_t*)(PWR_BASE + 0x00))
#define PWR_CR_VOS_Pos  14
#define PWR_CR_VOS_SCALE1 (0x3U << PWR_CR_VOS_Pos)  /* Scale 1: up to 168MHz */

/* ================================================================
 * Public API
 * ================================================================ */

void rcc_init(void)
{
#ifdef SIMULATION
    /*
     * Simulation path: Use HSI (16MHz internal RC) directly.
     * QEMU has no real HSE crystal, so HSE/HSERDY never asserts.
     * No PLL needed at 16MHz. Flash: 0 wait states.
     *
     * HSI = 16MHz -> SYSCLK = 16MHz, HCLK = 16MHz, PCLK1 = 16MHz, PCLK2 = 16MHz.
     */
    RCC_CR |= RCC_CR_HSION;
    while (!(RCC_CR & RCC_CR_HSIRDY));

    uint32_t cfgr = RCC_CFGR;
    cfgr &= ~RCC_CFGR_SW_Msk;
    cfgr |= RCC_CFGR_SW_HSI;
    RCC_CFGR = cfgr;

    cfgr = RCC_CFGR;
    cfgr &= ~(0xFU << RCC_CFGR_HPRE_Pos);
    cfgr &= ~(0x7U << RCC_CFGR_PPRE1_Pos);
    cfgr &= ~(0x7U << RCC_CFGR_PPRE2_Pos);
    RCC_CFGR = cfgr;

    FLASH_ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
#else
    /*
     * Hardware path: HSE (8MHz) -> PLL -> 168MHz SYSCLK.
     */
    /* 1. Enable PWR peripheral clock (needed for VOS setting) */
    RCC_APB1ENR |= (1U << 28);

    /* 2. Set voltage regulator to Scale 1 (required for 168MHz) */
    PWR_CR |= PWR_CR_VOS_SCALE1;

    /* 3. Enable HSE (8MHz external crystal) */
    RCC_CR |= RCC_CR_HSEON;
    while (!(RCC_CR & RCC_CR_HSERDY));

    /* 4. Configure PLL:
     * PLLM=8, PLLN=336, PLLP=2 (div2), PLLSRC=HSE
     * VCO = 8MHz / 8 * 336 = 336MHz
     * SYSCLK = 336 / 2 = 168MHz
     */
    uint32_t pllcfgr = 0;
    pllcfgr |= (8 << RCC_PLLCFGR_PLLM_Pos);
    pllcfgr |= (336 << RCC_PLLCFGR_PLLN_Pos);
    pllcfgr |= RCC_PLLCFGR_PLLP_DIV2;
    pllcfgr |= RCC_PLLCFGR_PLLSRC_HSE;
    RCC_PLLCFGR = pllcfgr;

    /* 5. Set Flash wait states */
    FLASH_ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* 6. Configure AHB/APB prescalers */
    uint32_t cfgr = RCC_CFGR;
    cfgr &= ~(0xFU << RCC_CFGR_HPRE_Pos);
    cfgr &= ~(0x7U << RCC_CFGR_PPRE1_Pos);
    cfgr &= ~(0x7U << RCC_CFGR_PPRE2_Pos);
    cfgr |= RCC_CFGR_HPRE_DIV1;
    cfgr |= RCC_CFGR_PPRE1_DIV4;
    cfgr |= RCC_CFGR_PPRE2_DIV2;
    RCC_CFGR = cfgr;

    /* 7. Enable PLL */
    RCC_CR |= RCC_CR_PLLON;
    while (!(RCC_CR & RCC_CR_PLLRDY));

    /* 8. Switch system clock to PLL */
    cfgr = RCC_CFGR;
    cfgr &= ~RCC_CFGR_SW_Msk;
    cfgr |= RCC_CFGR_SW_PLL;
    RCC_CFGR = cfgr;
    while ((RCC_CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL);
#endif
}

void rcc_enable_gpio(void)
{
    /* On STM32F4, GPIO clock enable is on AHB1ENR (NOT APB2ENR like F1) */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
}

void rcc_enable_i2c1(void)
{
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;
}

void rcc_enable_spi1(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_SPI1EN;
}
