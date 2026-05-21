#include "uart.h"
#include "stm32f103.h"

/* USART1 Base Address */
#define USART1_BASE   0x40013800U
#define USART1_SR     (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define USART1_DR     (*(volatile uint32_t *)(USART1_BASE + 0x04))
#define USART1_BRR    (*(volatile uint32_t *)(USART1_BASE + 0x08))
#define USART1_CR1    (*(volatile uint32_t *)(USART1_BASE + 0x0C))
#define USART1_CR2    (*(volatile uint32_t *)(USART1_BASE + 0x10))
#define USART1_CR3    (*(volatile uint32_t *)(USART1_BASE + 0x14))

/* USART_SR bits */
#define USART_SR_TXE  (1U << 7)
#define USART_SR_TC   (1U << 6)
#define USART_SR_RXNE (1U << 5)

/* USART_CR1 bits */
#define USART_CR1_UE  (1U << 13)
#define USART_CR1_M   (1U << 12)
#define USART_CR1_TE  (1U << 3)
#define USART_CR1_RE  (1U << 2)

/* USART1 pins: PA9=TX, PA10=RX */

void uart1_init(void) {
    /* Enable GPIOA clock (APB2) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    /* Enable USART1 clock (APB2) */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* Configure PA9 (TX): Alternate Function Push-Pull, 50MHz */
    {
        uint32_t crh = GPIOA->CRH;
        crh &= ~(0xFU << ((9 - 8) * 4));
        crh |= GPIO_CRH_PIN_CFG(9, GPIO_MODE_OUT50, GPIO_CNF_AF_PP);
        GPIOA->CRH = crh;
    }

    /* Configure PA10 (RX): Input floating (optional for TX-only) */
    {
        uint32_t crh = GPIOA->CRH;
        crh &= ~(0xFU << ((10 - 8) * 4));
        crh |= GPIO_CRH_PIN_CFG(10, GPIO_MODE_INPUT, GPIO_CNF_FLOAT_IN);
        GPIOA->CRH = crh;
    }

    /* USART1: 115200 baud, 8N1
     * APB2 clock = 8 MHz (HSI)
     * BRR = 8,000,000 / (16 * 115,200) = 4.34 => 4 = 0x0004
     * For 8MHz HSI: 8000000 / 115200 = 69.44 => mantissa=4, fraction=5 => 0x045
     * Actually: BRR = PCLK / (16 * Baud)
     * With HSI 8MHz: BRR = 8,000,000 / (16 * 115,200) = 4.340
     * Mantissa = 4 (integer part), Fraction = round(0.340 * 16) = 5
     * BRR = (4 << 4) | 5 = 0x0045
     */
    USART1_BRR = 0x0045;

    /* Enable USART, TX only */
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;
}

void uart1_putc(char c) {
    /* Wait until TX buffer empty */
    while (!(USART1_SR & USART_SR_TXE)) {}
    USART1_DR = c;
}

void uart1_puts(const char *s) {
    while (*s) {
        uart1_putc(*s++);
    }
}

static const char hexchars[] = "0123456789ABCDEF";

static void uart1_print_hex32(uint32_t val) {
    uart1_putc('0');
    uart1_putc('x');
    for (int i = 28; i >= 0; i -= 4) {
        uart1_putc(hexchars[(val >> i) & 0xF]);
    }
}

void uart1_print_reg(const char *name, uint32_t val, uint32_t expected_mask,
                     uint32_t expected_val) {
    uart1_putc('[');
    if ((val & expected_mask) == expected_val) {
        uart1_puts("PASS");
    } else {
        uart1_puts("FAIL");
    }
    uart1_puts("] ");
    uart1_puts(name);
    uart1_putc('\n');

    if ((val & expected_mask) != expected_val) {
        uart1_puts("  Expected: ");
        uart1_print_hex32(expected_val);
        uart1_puts(", Actual: ");
        uart1_print_hex32(val);
        uart1_putc('\n');
    }
}
