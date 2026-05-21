#include "spi_driver.h"

void spi1_init(void) {
    /* Enable GPIOA and GPIOB clocks (APB2) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;

    /* Enable SPI1 clock (APB2) */
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    /* Configure PA5 (SCK): Alternate Function Push-Pull, 50MHz */
    {
        uint32_t crl = GPIOA->CRL;
        crl &= ~(0xFU << (5 * 4));
        crl |= GPIO_CRL_PIN_CFG(5, GPIO_MODE_OUT50, GPIO_CNF_AF_PP);
        GPIOA->CRL = crl;
    }

    /* Configure PA7 (MOSI): Alternate Function Push-Pull, 50MHz */
    {
        uint32_t crl = GPIOA->CRL;
        crl &= ~(0xFU << (7 * 4));
        crl |= GPIO_CRL_PIN_CFG(7, GPIO_MODE_OUT50, GPIO_CNF_AF_PP);
        GPIOA->CRL = crl;
    }

    /* Configure PA4 (DC): General Purpose Push-Pull Output, 50MHz */
    {
        uint32_t crl = GPIOA->CRL;
        crl &= ~(0xFU << (4 * 4));
        crl |= GPIO_CRL_PIN_CFG(4, GPIO_MODE_OUT50, GPIO_CNF_PP_OUT);
        GPIOA->CRL = crl;
    }

    /* Configure PB0 (RES): General Purpose Push-Pull Output, 50MHz */
    {
        uint32_t crl = GPIOB->CRL;
        crl &= ~(0xFU << (0 * 4));
        crl |= GPIO_CRL_PIN_CFG(0, GPIO_MODE_OUT50, GPIO_CNF_PP_OUT);
        GPIOB->CRL = crl;
    }

    /* Configure SPI1:
     * - Master mode
     * - Baud rate = fPCLK2 / 32 = 72MHz / 32 = 2.25 MHz
     * - CPOL = 0, CPHA = 0 (Mode 0: idle low, capture on rising edge)
     * - MSB first, 8-bit data
     * - Software slave management (SSM=1, SSI=1)
     */
    SPI1->CR1 = SPI_CR1_MSTR
              | (SPI_CR1_BR_DIV32 << SPI_CR1_BR_SHIFT)
              | SPI_CR1_SSM
              | SPI_CR1_SSI;

    /* Enable SPI1 */
    SPI1->CR1 |= SPI_CR1_SPE;
}

void spi1_dc_command(void) {
    GPIOA->BRR = (1U << OLED_DC_PIN);  /* DC low for command */
}

void spi1_dc_data(void) {
    GPIOA->BSRR = (1U << OLED_DC_PIN); /* DC high for data */
}

void spi1_res_low(void) {
    GPIOB->BRR = (1U << OLED_RES_PIN);
}

void spi1_res_high(void) {
    GPIOB->BSRR = (1U << OLED_RES_PIN);
}

void spi1_cs_low(void) {
    /* CS not connected separately for 6-pin OLED; handled by SSM=SSI=1 */
}

void spi1_cs_high(void) {
    /* CS not connected separately for 6-pin OLED; handled by SSM=SSI=1 */
}

void spi1_send_byte(uint8_t data) {
    /* Wait until TXE (transmit buffer empty) */
    while (!(SPI1->SR & SPI_SR_TXE)) {}
    /* Send byte */
    SPI1->DR = data;
    /* Wait until RXNE (receive buffer not empty) to ensure transfer complete */
    while (!(SPI1->SR & SPI_SR_RXNE)) {}
    /* Read DR to clear RXNE */
    (void)SPI1->DR;
    /* Wait until not busy */
    while (SPI1->SR & SPI_SR_BSY) {}
}

void spi1_send_bytes(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        spi1_send_byte(data[i]);
    }
}

uint8_t spi1_transfer(uint8_t data) {
    /* Wait until TXE */
    while (!(SPI1->SR & SPI_SR_TXE)) {}
    /* Send byte */
    SPI1->DR = data;
    /* Wait until RXNE */
    while (!(SPI1->SR & SPI_SR_RXNE)) {}
    /* Wait until not busy */
    while (SPI1->SR & SPI_SR_BSY) {}
    return (uint8_t)(SPI1->DR & 0xFF);
}
