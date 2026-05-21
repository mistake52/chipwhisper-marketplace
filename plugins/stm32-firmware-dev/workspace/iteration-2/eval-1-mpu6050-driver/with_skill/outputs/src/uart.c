/**
 * uart.c — USART1: 115200 baud, 8N1, PA9 TX only
 *
 * MCP-verified register layout (RM0008 Rev 21, sec 27.6):
 *   USART1_SR:  0x40013800+0x00, TXE=bit7, TC=bit6, reset=0x00C0
 *   USART1_DR:  0x40013800+0x04
 *   USART1_BRR: 0x40013800+0x08
 *   USART1_CR1: 0x40013800+0x0C, UE=bit13, TE=bit3, RE=bit2
 *
 * Baud rate: BRR = APB2_FREQ / BAUD = 72000000 / 115200 = 625 = 0x0271
 */

#include "uart.h"
#include "rcc.h"
#include "main.h"
#include <stdint.h>

/* --- USART1 Register Definitions (MCP-verified) --- */
#define USART1_BASE     0x40013800UL
#define USART1_SR       (*(volatile uint32_t*)(USART1_BASE + 0x00))
#define USART1_DR       (*(volatile uint32_t*)(USART1_BASE + 0x04))
#define USART1_BRR      (*(volatile uint32_t*)(USART1_BASE + 0x08))
#define USART1_CR1      (*(volatile uint32_t*)(USART1_BASE + 0x0C))
#define USART1_CR2      (*(volatile uint32_t*)(USART1_BASE + 0x10))
#define USART1_CR3      (*(volatile uint32_t*)(USART1_BASE + 0x14))
#define USART1_GTPR     (*(volatile uint32_t*)(USART1_BASE + 0x18))

/* USART1_SR bit definitions */
#define USART_SR_TXE    (1UL << 7)
#define USART_SR_TC     (1UL << 6)

/* USART1_CR1 bit definitions */
#define USART_CR1_UE    (1UL << 13)
#define USART_CR1_TE    (1UL << 3)
#define USART_CR1_RE    (1UL << 2)

/* --- GPIOA Register Definitions --- */
#define GPIOA_BASE      0x40010800UL
#define GPIOA_CRL       (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
#define GPIOA_CRH       (*(volatile uint32_t*)(GPIOA_BASE + 0x04))

void uart_init(void)
{
    /* 1. Enable GPIOA and USART1 clocks on APB2 */
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* 2. Configure PA9 as alternate function push-pull, 50MHz */
    /*    PA9 is in CRH bits [7:4]. CNF=10 (AF_PP), MODE=11 (50MHz) = 0xB */
    {
        uint32_t crh = GPIOA_CRH;
        crh &= ~(0xFU << 4);       /* Clear bits [7:4] */
        crh |=  (0xBU << 4);       /* CNF=10, MODE=11 */
        GPIOA_CRH = crh;
    }

    /* 3. Configure USART1: 115200 baud, 8N1 */
    /*    BRR = APB2_FREQ / BAUD = 72000000 / 115200 = 625 = 0x0271 */
    USART1_BRR = APB2_FREQ / 115200;

    /*    CR1: enable TX, RX (for completeness), and USART */
    USART1_CR1 = USART_CR1_TE | USART_CR1_RE;
    USART1_CR1 |= USART_CR1_UE;   /* Enable UE last (per RM0008 sec 27.6.4) */
}

void uart_putc(char c)
{
    /* Wait until TXE (Transmit Data Register Empty) is set */
    while (!(USART1_SR & USART_SR_TXE));
    USART1_DR = c;
}

void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

void uart_print_int(int32_t val)
{
    char buf[12];
    int i = 0;
    uint32_t uval;

    if (val < 0) {
        uart_putc('-');
        uval = (uint32_t)(-val);
    } else {
        uval = (uint32_t)val;
    }

    if (uval == 0) {
        uart_putc('0');
        return;
    }

    while (uval > 0) {
        buf[i++] = '0' + (uval % 10);
        uval /= 10;
    }

    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

void uart_print_float(float val, int32_t decimals)
{
    if (val < 0.0f) {
        uart_putc('-');
        val = -val;
    }

    /* Integer part */
    int32_t int_part = (int32_t)val;
    uart_print_int(int_part);

    /* Fractional part */
    if (decimals > 0) {
        uart_putc('.');
        float frac = val - (float)int_part;
        while (decimals-- > 0) {
            frac *= 10.0f;
            int digit = (int)frac;
            uart_putc('0' + digit);
            frac -= (float)digit;
        }
    }
}
