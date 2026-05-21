#include "i2c_driver.h"

static uint32_t g_last_sr1 = 0;

/* Delay for ~1us per loop iteration at 72MHz */
static void delay_us(uint32_t us) {
    for (volatile uint32_t i = 0; i < us * 12; i++) {
        __asm__ volatile ("nop");
    }
}

/* Wait for I2C event with timeout (~1ms max) */
static int i2c1_wait_flag(uint32_t flag, int set) {
    uint32_t timeout = 100000;
    while (timeout--) {
        uint32_t sr1 = I2C1->SR1;
        if (set) {
            if (sr1 & flag) return 0;
        } else {
            if (!(sr1 & flag)) return 0;
        }
    }
    g_last_sr1 = I2C1->SR1;
    return -1;
}

/* Wait for SB (start condition sent) */
static int i2c1_wait_sb(void) {
    return i2c1_wait_flag(I2C_SR1_SB, 1);
}

/* Wait for ADDR (address sent) */
static int i2c1_wait_addr(void) {
    return i2c1_wait_flag(I2C_SR1_ADDR, 1);
}

/* Wait for TXE (data register empty) */
static int i2c1_wait_txe(void) {
    return i2c1_wait_flag(I2C_SR1_TXE, 1);
}

/* Wait for RXNE (data register not empty) */
static int i2c1_wait_rxne(void) {
    return i2c1_wait_flag(I2C_SR1_RXNE, 1);
}

/* Wait for BTF (byte transfer finished) */
static int i2c1_wait_btf(void) {
    return i2c1_wait_flag(I2C_SR1_BTF, 1);
}

void i2c1_init(void) {
    /* Enable GPIOB clock (APB2) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    /* Enable I2C1 clock (APB1) */
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* Configure PB6 (SCL) and PB7 (SDA) as Alternate Function Open Drain, 50MHz */
    uint32_t crl = GPIOB->CRL;
    /* Clear bits for PB6 */
    crl &= ~(0xFU << (I2C1_SCL_PIN * 4));
    crl |= GPIO_CRL_PIN_CFG(I2C1_SCL_PIN, GPIO_MODE_OUT50, GPIO_CNF_AF_OD);
    /* Clear bits for PB7 */
    crl &= ~(0xFU << (I2C1_SDA_PIN * 4));
    crl |= GPIO_CRL_PIN_CFG(I2C1_SDA_PIN, GPIO_MODE_OUT50, GPIO_CNF_AF_OD);
    GPIOB->CRL = crl;

    /* Reset I2C1 */
    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    /* Set peripheral clock frequency: PCLK1 = 36 MHz */
    I2C1->CR2 = (36 & I2C_CR2_FREQ_MASK);

    /* Configure for 100 kHz standard mode:
     * CCR = PCLK1 / (2 * f_SCL) = 36MHz / (2 * 100kHz) = 180
     */
    I2C1->CCR = 180;

    /* TRISE = (1000ns / T_PCLK1) + 1 = (1000ns / 27.78ns) + 1 = 36 + 1 = 37
     * For standard mode max rise time is 1000ns, T_PCLK1 = 1/36MHz = 27.78ns
     */
    I2C1->TRISE = 37;

    /* Enable I2C1 peripheral */
    I2C1->CR1 |= I2C_CR1_PE;
}

int i2c1_probe(uint8_t dev_addr) {
    uint32_t cr1;

    /* Send START condition */
    I2C1->CR1 |= I2C_CR1_START;
    if (i2c1_wait_sb() != 0) {
        return -1;
    }

    /* Send 7-bit slave address with write bit (bit 0 = 0) */
    I2C1->DR = (dev_addr << 1);
    if (i2c1_wait_addr() != 0) {
        return -1;
    }

    /* Read SR1 and SR2 to clear ADDR flag */
    cr1 = I2C1->SR1;
    cr1 = I2C1->SR2;
    (void)cr1;

    /* Check if slave acknowledged */
    if (I2C1->SR1 & I2C_SR1_AF) {
        I2C1->SR1 &= ~I2C_SR1_AF;
        I2C1->CR1 |= I2C_CR1_STOP;
        return -1;
    }

    /* Send STOP condition */
    I2C1->CR1 |= I2C_CR1_STOP;
    return 0;
}

int i2c1_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data) {
    /* Send START */
    I2C1->CR1 |= I2C_CR1_START;
    if (i2c1_wait_sb() != 0) return -1;

    /* Send device address (write) */
    I2C1->DR = (uint8_t)(dev_addr << 1);
    if (i2c1_wait_addr() != 0) return -1;

    /* Clear ADDR */
    volatile uint32_t dummy = I2C1->SR1;
    dummy = I2C1->SR2;
    (void)dummy;

    /* Send register address */
    I2C1->DR = reg;
    if (i2c1_wait_txe() != 0) return -1;

    /* Send data byte */
    I2C1->DR = data;
    if (i2c1_wait_btf() != 0) return -1;

    /* Send STOP */
    I2C1->CR1 |= I2C_CR1_STOP;
    delay_us(10); /* Wait for STOP to complete */
    return 0;
}

int i2c1_write_byte(uint8_t dev_addr, uint8_t data) {
    /* Send START */
    I2C1->CR1 |= I2C_CR1_START;
    if (i2c1_wait_sb() != 0) return -1;

    /* Send device address (write) */
    I2C1->DR = (uint8_t)(dev_addr << 1);
    if (i2c1_wait_addr() != 0) return -1;

    /* Clear ADDR */
    volatile uint32_t dummy = I2C1->SR1;
    dummy = I2C1->SR2;
    (void)dummy;

    /* Send data byte */
    I2C1->DR = data;
    if (i2c1_wait_btf() != 0) return -1;

    /* Send STOP */
    I2C1->CR1 |= I2C_CR1_STOP;
    return 0;
}

int i2c1_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data) {
    return i2c1_read_regs(dev_addr, reg, data, 1);
}

int i2c1_read_regs(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint8_t len) {
    if (len == 0) return 0;

    /* Phase 1: Write register address */
    /* Send START */
    I2C1->CR1 |= I2C_CR1_START;
    if (i2c1_wait_sb() != 0) return -1;

    /* Send device address (write) */
    I2C1->DR = (uint8_t)(dev_addr << 1);
    if (i2c1_wait_addr() != 0) return -1;

    /* Clear ADDR */
    volatile uint32_t dummy = I2C1->SR1;
    dummy = I2C1->SR2;
    (void)dummy;

    /* Send register address */
    I2C1->DR = reg;
    if (i2c1_wait_btf() != 0) return -1;

    /* Phase 2: Repeated START + read */
    I2C1->CR1 |= I2C_CR1_START;
    if (i2c1_wait_sb() != 0) return -1;

    /* Send device address (read) */
    I2C1->DR = (uint8_t)((dev_addr << 1) | 1);
    if (i2c1_wait_addr() != 0) return -1;

    /* Set up ACK for multi-byte reads */
    if (len == 1) {
        /* Single byte: NACK after address */
        I2C1->CR1 &= ~I2C_CR1_ACK;
        /* Clear ADDR */
        dummy = I2C1->SR1;
        dummy = I2C1->SR2;
        (void)dummy;
        /* Send STOP early for single byte */
        I2C1->CR1 |= I2C_CR1_STOP;
        /* Wait for data */
        if (i2c1_wait_rxne() != 0) return -1;
        *data = (uint8_t)(I2C1->DR & 0xFF);
    } else {
        /* Multi-byte: ACK enabled */
        I2C1->CR1 |= I2C_CR1_ACK;
        /* Clear ADDR */
        dummy = I2C1->SR1;
        dummy = I2C1->SR2;
        (void)dummy;

        for (uint8_t i = 0; i < len; i++) {
            if (i == len - 2) {
                /* Before second-to-last byte: disable ACK */
                while (!(I2C1->SR1 & I2C_SR1_BTF)) {}
                /* Wait for BTF before sending STOP to ensure last byte is clocked */
                I2C1->CR1 &= ~I2C_CR1_ACK;
                /* Read second-to-last byte */
                data[i] = (uint8_t)(I2C1->DR & 0xFF);
                /* Send STOP */
                I2C1->CR1 |= I2C_CR1_STOP;
            } else if (i == len - 1) {
                /* Last byte: read after STOP */
                if (i2c1_wait_rxne() != 0) return -1;
                data[i] = (uint8_t)(I2C1->DR & 0xFF);
            } else {
                /* Normal byte */
                if (i2c1_wait_rxne() != 0) return -1;
                data[i] = (uint8_t)(I2C1->DR & 0xFF);
            }
        }
    }

    return 0;
}

uint32_t i2c1_get_last_sr1(void) {
    return g_last_sr1;
}
