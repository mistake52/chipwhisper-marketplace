/**
 * rcc.c — System clock configuration
 *
 * MCP-Verified register values from RM0008 Rev 21:
 *   RCC_BASE   = 0x40021000 (AHB bus)
 *     CR       @ 0x00, reset 0x00000083
 *     CFGR     @ 0x04, reset 0x00000000
 *     CIR      @ 0x08, reset 0x00000000
 *     APB2RSTR @ 0x0C, reset 0x00000000
 *     APB1RSTR @ 0x10, reset 0x00000000
 *     AHBENR   @ 0x14, reset 0x00000014
 *     APB2ENR  @ 0x18, reset 0x00000000
 *     APB1ENR  @ 0x1C, reset 0x00000000
 *     BDCR     @ 0x20, reset 0x00000000
 *     CSR      @ 0x24, reset 0x0C000000
 *   FLASH_BASE = 0x40022000 (AHB bus)
 *     ACR      @ 0x00, reset 0x00000030
 *
 * Hardware config (F103 @ 72MHz):
 *   HSE=8MHz → PLL×9 → SYSCLK=72MHz → AHB=72MHz, APB1=36MHz, APB2=72MHz
 *   FLASH_ACR LATENCY=2
 *
 * Simulation config (F100 via stm32vldiscovery):
 *   HSI=8MHz → SYSCLK=8MHz, no PLL
 */

#include "rcc.h"
#include "main.h"

/* RCC register definitions */
#define RCC_BASE      0x40021000UL
#define RCC_CR        (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_CFGR      (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_CIR       (*(volatile uint32_t*)(RCC_BASE + 0x08))
#define RCC_APB2RSTR  (*(volatile uint32_t*)(RCC_BASE + 0x0C))
#define RCC_APB1RSTR  (*(volatile uint32_t*)(RCC_BASE + 0x10))
#define RCC_AHBENR    (*(volatile uint32_t*)(RCC_BASE + 0x14))
#define RCC_APB2ENR   (*(volatile uint32_t*)(RCC_BASE + 0x18))
#define RCC_APB1ENR   (*(volatile uint32_t*)(RCC_BASE + 0x1C))
#define RCC_BDCR      (*(volatile uint32_t*)(RCC_BASE + 0x20))
#define RCC_CSR       (*(volatile uint32_t*)(RCC_BASE + 0x24))

/* FLASH register definitions */
#define FLASH_BASE    0x40022000UL
#define FLASH_ACR     (*(volatile uint32_t*)(FLASH_BASE + 0x00))

/* RCC_CR bit definitions */
#define RCC_CR_HSION      (1UL << 0)
#define RCC_CR_HSIRDY     (1UL << 1)
#define RCC_CR_HSEON      (1UL << 16)
#define RCC_CR_HSERDY     (1UL << 17)
#define RCC_CR_HSEBYP     (1UL << 18)
#define RCC_CR_CSSON      (1UL << 19)
#define RCC_CR_PLLON      (1UL << 24)
#define RCC_CR_PLLRDY     (1UL << 25)

/* RCC_CFGR bit definitions */
#define RCC_CFGR_SW_PLL     0x02  /* SW = 10: PLL as system clock */
#define RCC_CFGR_SWS_PLL    0x08  /* SWS = 10 (shifted to bits 2:3) */
#define RCC_CFGR_PLLSRC_HSE (1UL << 16)   /* PLL clock source = HSE */
#define RCC_CFGR_PLLXTPRE   (1UL << 17)   /* HSE divider for PLL (0=/1, 1=/2) */
#define RCC_CFGR_PLLMUL_9   (0x07UL << 18) /* PLLMUL = 0111 → ×9 */
#define RCC_CFGR_PPRE1_DIV2 (0x04UL << 8)  /* PPRE1=100 → APB1 ÷ 2  */
#define RCC_CFGR_PPRE2_DIV1 (0x00UL << 11) /* PPRE2=000 → APB2 no divide */
#define RCC_CFGR_HPRE_DIV1  (0x00UL << 4)  /* HPRE=0xxx → AHB no divide */

/* RCC_APB2ENR bit definitions (MCP-verified) */
#define RCC_APB2ENR_AFIOEN  (1UL << 0)   /* Alternate function I/O clock */
#define RCC_APB2ENR_IOPAEN  (1UL << 2)   /* GPIOA clock */
#define RCC_APB2ENR_IOPBEN  (1UL << 3)   /* GPIOB clock */
#define RCC_APB2ENR_IOPCEN  (1UL << 4)   /* GPIOC clock */
#define RCC_APB2ENR_SPI1EN  (1UL << 12)  /* SPI1 clock */

/* RCC_APB1ENR bit definitions (MCP-verified) */
#define RCC_APB1ENR_TIM2EN  (1UL << 0)   /* TIM2 clock */
#define RCC_APB1ENR_I2C1EN  (1UL << 21)  /* I2C1 clock */

/* RCC_AHBENR bit definitions */
#define RCC_AHBENR_DMA1EN   (1UL << 0)   /* DMA1 clock */
#define RCC_AHBENR_FLITFEN  (1UL << 4)   /* FLITF (flash interface) clock */

void rcc_init(void) {
#ifdef SIMULATION
    /* Simulation mode: use HSI 8MHz directly, no PLL */
    /* HSI is already enabled at reset (CR reset=0x00000083) */
    /* Wait for HSIRDY just in case */
    while (!(RCC_CR & RCC_CR_HSIRDY)) {
        __asm__ volatile ("nop");
    }
    /* HSI is already selected as system clock (SW=00 at reset) */
#else
    /* Hardware mode: HSE 8MHz → PLL ×9 → 72MHz */

    /* Step 1: Set FLASH latency to 2 wait states for 72MHz (MCP-verified: ACR @ 0x40022000+0x00) */
    FLASH_ACR = (FLASH_ACR & ~0x07UL) | 0x02UL;  /* LATENCY=010 */

    /* Step 2: Enable HSE */
    RCC_CR |= RCC_CR_HSEON;
    while (!(RCC_CR & RCC_CR_HSERDY)) {
        __asm__ volatile ("nop");
    }

    /* Step 3: Configure PLL */
    /*   PLLSRC = HSE (bit 16 = 1)
     *   PLLXTPRE = /1 (bit 17 = 0)
     *   PLLMUL = ×9 (bits 18-21 = 0111)
     */
    uint32_t cfgr = RCC_CFGR;
    cfgr &= ~((1UL << 16) | (1UL << 17) | (0x0FUL << 18));  /* Clear PLL fields */
    cfgr |= RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMUL_9;        /* PLLSRC=HSE, PLLMUL=×9 */
    /* Set APB1 prescaler ÷2 (max 36MHz), APB2 no divide */
    cfgr &= ~((0x07UL << 8) | (0x07UL << 11) | (0x0FUL << 4));
    cfgr |= RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_HPRE_DIV1;
    RCC_CFGR = cfgr;

    /* Step 4: Enable PLL */
    RCC_CR |= RCC_CR_PLLON;
    while (!(RCC_CR & RCC_CR_PLLRDY)) {
        __asm__ volatile ("nop");
    }

    /* Step 5: Switch system clock to PLL */
    cfgr = RCC_CFGR;
    cfgr &= ~0x03UL;       /* Clear SW bits 0:1 */
    cfgr |= RCC_CFGR_SW_PLL;  /* SW = 10 */
    RCC_CFGR = cfgr;

    /* Step 6: Wait for PLL to become system clock (SWS = 10) */
    while ((RCC_CFGR & 0x0CUL) != RCC_CFGR_SWS_PLL) {
        __asm__ volatile ("nop");
    }
#endif

    /* Enable peripheral clocks (common to both HW and simulation) */
    /* APB2: GPIOA (PA5=SCK, PA7=MOSI), GPIOB (PB0=RES, PB1=DC, PB6=SCL, PB7=SDA),
     *       GPIOC, SPI1 */
    /* MCP-verified: APB2ENR @ RCC_BASE+0x18, APB1ENR @ RCC_BASE+0x1C */
    RCC_APB2ENR |= (RCC_APB2ENR_AFIOEN  |
                    RCC_APB2ENR_IOPAEN  |
                    RCC_APB2ENR_IOPBEN  |
                    RCC_APB2ENR_IOPCEN  |
                    RCC_APB2ENR_SPI1EN);

    /* APB1: I2C1 */
    /* MCP-verified: I2C1EN=bit21 in APB1ENR */
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* Small delay for peripheral clocks to stabilize */
    for (volatile int i = 0; i < 100; i++) {
        __asm__ volatile ("nop");
    }
}
