/**
 * spi_bus.c — SPI1 bus driver for STM32F103
 *
 * MCP-Verified register addresses (RM0008 Rev 21, P787-821):
 *   SPI1_BASE = 0x40013000
 *     CR1     @ 0x00 — CPHA(bit0), CPOL(bit1), MSTR(bit2), BR[3:5],
 *                       SPE(bit6), LSBFIRST(bit7), SSI(bit8), SSM(bit9),
 *                       RXONLY(bit10), DFF(bit11), BIDIMODE(bit15)
 *     CR2     @ 0x04 — TXDMAEN(bit1), RXDMAEN(bit0), SSOE(bit2)
 *     SR      @ 0x08 — RXNE(bit0), TXE(bit1), BSY(bit7)
 *     DR      @ 0x0C — data register (bits0:15)
 *
 * SPI1 GPIO: PA5=SCK, PA7=MOSI — AF push-pull, CNF=10 MODE=11 (4-bit value 0xB)
 *
 * OLED SPI pin:
 *   PA5 = SCK  (SPI1_SCK)
 *   PA7 = MOSI (SPI1_MOSI)
 *
 * CR1 value calculation (9MHz, master, CPOL=0, CPHA=0):
 *   SSM(bit9)=1, SSI(bit8)=1, SPE(bit6)=1, BR=010(fPCLK/8), MSTR(bit2)=1
 *   = (1<<9) | (1<<8) | (1<<6) | (2<<3) | (1<<2)
 *   = 0x200 | 0x100 | 0x40 | 0x10 | 0x04 = 0x354
 */

#include "spi_bus.h"
#include "main.h"

/* SPI1 register definitions (MCP-verified) */
#define SPI1_BASE  0x40013000UL
#define SPI1_CR1   (*(volatile uint32_t*)(SPI1_BASE + 0x00))
#define SPI1_CR2   (*(volatile uint32_t*)(SPI1_BASE + 0x04))
#define SPI1_SR    (*(volatile uint32_t*)(SPI1_BASE + 0x08))
#define SPI1_DR    (*(volatile uint32_t*)(SPI1_BASE + 0x0C))

/* SR bits */
#define SPI_SR_RXNE  (1UL << 0)
#define SPI_SR_TXE   (1UL << 1)
#define SPI_SR_BSY   (1UL << 7)

/* STM32F103 GPIOA base (MCP-verified) */
#define GPIOA_BASE  0x40010800UL
#define GPIOA_CRL   (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
#define GPIOA_CRH   (*(volatile uint32_t*)(GPIOA_BASE + 0x04))
#define GPIOA_ODR   (*(volatile uint32_t*)(GPIOA_BASE + 0x0C))

/* GPIO configuration for SPI pins:
 * PA5=SCK: AF push-pull, 50MHz → CNF=10 MODE=11 → 0xB, bits 23:20
 * PA7=MOSI: AF push-pull, 50MHz → CNF=10 MODE=11 → 0xB, bits 31:28
 */
#define SPI_GPIO_CFG 0xB

void spi_init(void) {
    /* Configure PA5 (SCK) and PA7 (MOSI) as AF push-pull */
    /* PA5: CRL bits 23:20, PA7: CRL bits 31:28 */
    uint32_t crl = GPIOA_CRL;
    crl &= ~((0xFUL << (5 * 4)) | (0xFUL << (7 * 4)));
    crl |= (SPI_GPIO_CFG << (5 * 4)) | (SPI_GPIO_CFG << (7 * 4));
    GPIOA_CRL = crl;

    /* Configure SPI1:
     * 0x0354 = SSM=1 | SSI=1 | SPE=1 | BR=fPCLK/8 | MSTR=1
     * MCP-verified: CR1 bit layout matches RM0008 Section 25.5.1
     */
    SPI1_CR1 = 0x0354;
}

void spi_write(uint8_t data) {
    /* Wait until TX buffer is empty */
    while (!(SPI1_SR & SPI_SR_TXE)) {
        __asm__ volatile ("nop");
    }
    /* Write data to DR, 8-bit access */
    SPI1_DR = data;
    /* Wait for transfer complete */
    while (SPI1_SR & SPI_SR_BSY) {
        __asm__ volatile ("nop");
    }
}

uint8_t spi_transfer(uint8_t data) {
    while (!(SPI1_SR & SPI_SR_TXE)) {
        __asm__ volatile ("nop");
    }
    SPI1_DR = data;
    while (!(SPI1_SR & SPI_SR_RXNE)) {
        __asm__ volatile ("nop");
    }
    while (SPI1_SR & SPI_SR_BSY) {
        __asm__ volatile ("nop");
    }
    return (uint8_t)(SPI1_DR & 0xFF);
}
