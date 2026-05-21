#ifndef STM32F103_H
#define STM32F103_H

#include <stdint.h>

/* --- Memory Map --- */
#define PERIPH_BASE          0x40000000U
#define APB1_PERIPH_BASE     0x40000000U
#define APB2_PERIPH_BASE     0x40010000U
#define AHB_PERIPH_BASE      0x40018000U /* actually AHB starts at 40018000 for DMA */

/* --- RCC --- */
#define RCC_BASE             (AHB_PERIPH_BASE + 0x9000)
#define RCC                  ((RCC_TypeDef *) RCC_BASE)

#define RCC_CR_OFFSET        0x00
#define RCC_CFGR_OFFSET      0x04
#define RCC_CIR_OFFSET       0x08
#define RCC_APB2RSTR_OFFSET  0x0C
#define RCC_APB1RSTR_OFFSET  0x10
#define RCC_AHBENR_OFFSET    0x14
#define RCC_APB2ENR_OFFSET   0x18
#define RCC_APB1ENR_OFFSET   0x1C

/* RCC_CR bits */
#define RCC_CR_HSION         (1U << 0)
#define RCC_CR_HSIRDY        (1U << 1)
#define RCC_CR_HSEON         (1U << 16)
#define RCC_CR_HSERDY        (1U << 17)
#define RCC_CR_PLLON         (1U << 24)
#define RCC_CR_PLLRDY        (1U << 25)

/* RCC_CFGR bits */
#define RCC_CFGR_SW_HSI      0
#define RCC_CFGR_SW_HSE      1
#define RCC_CFGR_SW_PLL      2
#define RCC_CFGR_SWS_SHIFT   2
#define RCC_CFGR_SWS_MASK    (0x3U << RCC_CFGR_SWS_SHIFT)

/* RCC_AHBENR bits */
#define RCC_AHBENR_DMA1EN    (1U << 0)

/* RCC_APB2ENR bits */
#define RCC_APB2ENR_AFIOEN   (1U << 0)
#define RCC_APB2ENR_IOPAEN   (1U << 2)
#define RCC_APB2ENR_IOPBEN   (1U << 3)
#define RCC_APB2ENR_IOPCEN   (1U << 4)
#define RCC_APB2ENR_ADC1EN   (1U << 9)
#define RCC_APB2ENR_SPI1EN   (1U << 12)
#define RCC_APB2ENR_USART1EN (1U << 14)

/* RCC_APB1ENR bits */
#define RCC_APB1ENR_TIM2EN   (1U << 0)
#define RCC_APB1ENR_USART2EN (1U << 17)
#define RCC_APB1ENR_I2C1EN   (1U << 21)

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t APB1RSTR;
    volatile uint32_t AHBENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
} RCC_TypeDef;

/* --- GPIO --- */
#define GPIOA_BASE           (APB2_PERIPH_BASE + 0x0800)
#define GPIOB_BASE           (APB2_PERIPH_BASE + 0x0C00)
#define GPIOC_BASE           (APB2_PERIPH_BASE + 0x1000)

#define GPIOA                ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOB                ((GPIO_TypeDef *) GPIOB_BASE)
#define GPIOC                ((GPIO_TypeDef *) GPIOC_BASE)

/* GPIO CRL/CRH mode bits */
#define GPIO_MODE_INPUT       0x0
#define GPIO_MODE_OUT10       0x1
#define GPIO_MODE_OUT2        0x2
#define GPIO_MODE_OUT50       0x3
#define GPIO_CNF_ANALOG       0x0
#define GPIO_CNF_FLOAT_IN     0x1
#define GPIO_CNF_PUPD_IN      0x2
#define GPIO_CNF_PP_OUT        0x0
#define GPIO_CNF_OD_OUT        0x1
#define GPIO_CNF_AF_PP         0x2
#define GPIO_CNF_AF_OD         0x3

/* GPIO pin configuration helpers */
#define GPIO_CRL_PIN_CFG(pin, mode, cnf) \
    ((uint32_t)(mode) | ((uint32_t)(cnf) << 2)) << ((pin) * 4)

#define GPIO_CRH_PIN_CFG(pin, mode, cnf) \
    ((uint32_t)(mode) | ((uint32_t)(cnf) << 2)) << (((pin) - 8) * 4)

typedef struct {
    volatile uint32_t CRL;
    volatile uint32_t CRH;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t BRR;
    volatile uint32_t LCKR;
} GPIO_TypeDef;

/* --- I2C1 --- */
#define I2C1_BASE            (APB1_PERIPH_BASE + 0x5400)
#define I2C1                 ((I2C_TypeDef *) I2C1_BASE)

/* I2C registers */
#define I2C_CR1_OFFSET       0x00
#define I2C_CR2_OFFSET       0x04
#define I2C_OAR1_OFFSET      0x08
#define I2C_OAR2_OFFSET      0x0C
#define I2C_DR_OFFSET        0x10
#define I2C_SR1_OFFSET       0x14
#define I2C_SR2_OFFSET       0x18
#define I2C_CCR_OFFSET       0x1C
#define I2C_TRISE_OFFSET     0x20

/* I2C_CR1 bits */
#define I2C_CR1_PE           (1U << 0)
#define I2C_CR1_SMBUS        (1U << 1)
#define I2C_CR1_SMBTYPE      (1U << 3)
#define I2C_CR1_ENARP        (1U << 4)
#define I2C_CR1_ENPEC        (1U << 5)
#define I2C_CR1_ENGC         (1U << 6)
#define I2C_CR1_NOSTRETCH    (1U << 7)
#define I2C_CR1_START        (1U << 8)
#define I2C_CR1_STOP         (1U << 9)
#define I2C_CR1_ACK          (1U << 10)
#define I2C_CR1_POS          (1U << 11)
#define I2C_CR1_PEC          (1U << 12)
#define I2C_CR1_ALERT        (1U << 13)
#define I2C_CR1_SWRST        (1U << 15)

/* I2C_CR2 bits */
#define I2C_CR2_FREQ_SHIFT   0
#define I2C_CR2_FREQ_MASK    (0x3FU << I2C_CR2_FREQ_SHIFT)
#define I2C_CR2_ITERREN      (1U << 8)
#define I2C_CR2_ITEVTEN      (1U << 9)
#define I2C_CR2_ITBUFEN      (1U << 10)
#define I2C_CR2_DMAEN        (1U << 11)
#define I2C_CR2_LAST         (1U << 12)

/* I2C_SR1 bits */
#define I2C_SR1_SB           (1U << 0)
#define I2C_SR1_ADDR         (1U << 1)
#define I2C_SR1_BTF          (1U << 2)
#define I2C_SR1_ADD10        (1U << 3)
#define I2C_SR1_STOPF        (1U << 4)
#define I2C_SR1_RXNE         (1U << 6)
#define I2C_SR1_TXE          (1U << 7)
#define I2C_SR1_BERR         (1U << 8)
#define I2C_SR1_ARLO         (1U << 9)
#define I2C_SR1_AF           (1U << 10)
#define I2C_SR1_OVR          (1U << 11)
#define I2C_SR1_PECERR       (1U << 12)
#define I2C_SR1_TIMEOUT      (1U << 14)
#define I2C_SR1_SMBALERT     (1U << 15)

/* I2C_SR2 bits */
#define I2C_SR2_MSL          (1U << 0)
#define I2C_SR2_BUSY         (1U << 1)
#define I2C_SR2_TRA          (1U << 2)
#define I2C_SR2_GENCALL      (1U << 4)
#define I2C_SR2_SMBDEFAULT   (1U << 5)
#define I2C_SR2_SMBHOST      (1U << 6)
#define I2C_SR2_DUALF        (1U << 7)
#define I2C_SR2_PEC_SHIFT    8

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t OAR1;
    volatile uint32_t OAR2;
    volatile uint32_t DR;
    volatile uint32_t SR1;
    volatile uint32_t SR2;
    volatile uint32_t CCR;
    volatile uint32_t TRISE;
} I2C_TypeDef;

/* --- SPI1 --- */
#define SPI1_BASE            (APB2_PERIPH_BASE + 0x3000)
#define SPI1                 ((SPI_TypeDef *) SPI1_BASE)

/* SPI_CR1 bits */
#define SPI_CR1_CPHA         (1U << 0)
#define SPI_CR1_CPOL         (1U << 1)
#define SPI_CR1_MSTR         (1U << 2)
#define SPI_CR1_BR_SHIFT     3
#define SPI_CR1_BR_MASK      (0x7U << SPI_CR1_BR_SHIFT)
#define SPI_CR1_BR_DIV2      0
#define SPI_CR1_BR_DIV4      1
#define SPI_CR1_BR_DIV8      2
#define SPI_CR1_BR_DIV16     3
#define SPI_CR1_BR_DIV32     4
#define SPI_CR1_BR_DIV64     5
#define SPI_CR1_BR_DIV128    6
#define SPI_CR1_BR_DIV256    7
#define SPI_CR1_SPE          (1U << 6)
#define SPI_CR1_LSBFIRST     (1U << 7)
#define SPI_CR1_SSI          (1U << 8)
#define SPI_CR1_SSM          (1U << 9)
#define SPI_CR1_RXONLY       (1U << 10)
#define SPI_CR1_DFF          (1U << 11)
#define SPI_CR1_CRCNEXT      (1U << 12)
#define SPI_CR1_CRCEN        (1U << 13)
#define SPI_CR1_BIDIOE       (1U << 14)
#define SPI_CR1_BIDIMODE     (1U << 15)

/* SPI_CR2 bits */
#define SPI_CR2_RXDMAEN      (1U << 0)
#define SPI_CR2_TXDMAEN      (1U << 1)
#define SPI_CR2_SSOE         (1U << 2)
#define SPI_CR2_ERRIE        (1U << 5)
#define SPI_CR2_RXNEIE       (1U << 6)
#define SPI_CR2_TXEIE        (1U << 7)

/* SPI_SR bits */
#define SPI_SR_RXNE          (1U << 0)
#define SPI_SR_TXE           (1U << 1)
#define SPI_SR_CHSIDE        (1U << 2)
#define SPI_SR_UDR           (1U << 3)
#define SPI_SR_CRCERR        (1U << 4)
#define SPI_SR_MODF          (1U << 5)
#define SPI_SR_OVR           (1U << 6)
#define SPI_SR_BSY           (1U << 7)

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t CRCPR;
    volatile uint32_t RXCRCR;
    volatile uint32_t TXCRCR;
    volatile uint32_t I2SCFGR;
    volatile uint32_t I2SPR;
} SPI_TypeDef;

/* --- SysTick --- */
#define SYSTICK_BASE         0xE000E010U
#define SYSTICK             ((SysTick_TypeDef *) SYSTICK_BASE)

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} SysTick_TypeDef;

#define SYSTICK_CTRL_ENABLE    (1U << 0)
#define SYSTICK_CTRL_TICKINT   (1U << 1)
#define SYSTICK_CTRL_CLKSOURCE (1U << 2)
#define SYSTICK_CTRL_COUNTFLAG (1U << 16)

/* --- NVIC --- */
#define NVIC_BASE             0xE000E100U
#define NVIC_ISER0            (*(volatile uint32_t *)(NVIC_BASE + 0x00))
#define NVIC_ISER1            (*(volatile uint32_t *)(NVIC_BASE + 0x04))

/* --- SCB --- */
#define SCB_BASE              0xE000ED00U
#define SCB_CPUID             (*(volatile uint32_t *)(SCB_BASE + 0x00))
#define SCB_ICSR              (*(volatile uint32_t *)(SCB_BASE + 0x04))
#define SCB_VTOR              (*(volatile uint32_t *)(SCB_BASE + 0x08))
#define SCB_AIRCR             (*(volatile uint32_t *)(SCB_BASE + 0x0C))
#define SCB_SCR               (*(volatile uint32_t *)(SCB_BASE + 0x10))
#define SCB_CCR               (*(volatile uint32_t *)(SCB_BASE + 0x14))

/* --- Flash --- */
#define FLASH_BASE            0x40022000U
#define FLASH_ACR             (*(volatile uint32_t *)(FLASH_BASE + 0x00))
#define FLASH_ACR_LATENCY_0   0
#define FLASH_ACR_LATENCY_1   (1U << 0)
#define FLASH_ACR_LATENCY_2   (2U << 0)
#define FLASH_ACR_PRFTBE      (1U << 4)

#endif /* STM32F103_H */
