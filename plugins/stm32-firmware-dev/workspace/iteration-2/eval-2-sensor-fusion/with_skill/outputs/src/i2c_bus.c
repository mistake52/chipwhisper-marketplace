/**
 * i2c_bus.c — I2C1 bus driver for STM32F103
 *
 * MCP-Verified register addresses and bitfields (RM0008 Rev 21, P772-784):
 *   I2C1_BASE = 0x40005400
 *     CR1   @ 0x00 — PE(bit0), START(bit8), STOP(bit9), ACK(bit10)
 *     CR2   @ 0x04 — FREQ(bits0:5)
 *     OAR1  @ 0x08 — own address
 *     OAR2  @ 0x0C — dual address
 *     DR    @ 0x10 — data register (bits0:7)
 *     SR1   @ 0x14 — SB(bit0), ADDR(bit1), BTF(bit2), TxE(bit7), RxNE(bit6)
 *     SR2   @ 0x18 — BUSY(bit1), TRA(bit2), MSL(bit0)
 *     CCR   @ 0x1C — CCR(bits0:11), DUTY(bit14), F_S(bit15)
 *     TRISE @ 0x20 — TRISE(bits0:5)
 *
 * I2C1 GPIO: PB6=SCL, PB7=SDA — AF open-drain, CNF=11 MODE=11 (4-bit value 0xF)
 */

#include "i2c_bus.h"
#include "main.h"

/* I2C1 register definitions */
#define I2C1_BASE  0x40005400UL
#define I2C1_CR1   (*(volatile uint32_t*)(I2C1_BASE + 0x00))
#define I2C1_CR2   (*(volatile uint32_t*)(I2C1_BASE + 0x04))
#define I2C1_OAR1  (*(volatile uint32_t*)(I2C1_BASE + 0x08))
#define I2C1_OAR2  (*(volatile uint32_t*)(I2C1_BASE + 0x0C))
#define I2C1_DR    (*(volatile uint32_t*)(I2C1_BASE + 0x10))
#define I2C1_SR1   (*(volatile uint32_t*)(I2C1_BASE + 0x14))
#define I2C1_SR2   (*(volatile uint32_t*)(I2C1_BASE + 0x18))
#define I2C1_CCR   (*(volatile uint32_t*)(I2C1_BASE + 0x1C))
#define I2C1_TRISE (*(volatile uint32_t*)(I2C1_BASE + 0x20))

/* SR1 flags */
#define I2C_SR1_SB     0x0001  /* Start bit sent */
#define I2C_SR1_ADDR   0x0002  /* Address sent/matched */
#define I2C_SR1_BTF    0x0004  /* Byte transfer finished */
#define I2C_SR1_ADD10  0x0008  /* 10-bit header sent */
#define I2C_SR1_STOPF  0x0010  /* Stop detection (slave) */
#define I2C_SR1_RxNE   0x0040  /* Receive buffer not empty */
#define I2C_SR1_TxE    0x0080  /* Transmit buffer empty */
#define I2C_SR1_BERR   0x0100  /* Bus error */
#define I2C_SR1_ARLO   0x0200  /* Arbitration lost */
#define I2C_SR1_AF     0x0400  /* Acknowledge failure */
#define I2C_SR1_OVR    0x0800  /* Overrun/underrun */
#define I2C_SR1_TIMEOUT 0x4000 /* Timeout */

/* SR2 flags */
#define I2C_SR2_MSL   0x0001   /* Master mode */
#define I2C_SR2_BUSY  0x0002   /* Bus busy */
#define I2C_SR2_TRA   0x0004   /* Transmitter/receiver */

/* CR1 bits */
#define I2C_CR1_PE     0x0001   /* Peripheral enable */
#define I2C_CR1_START  0x0100   /* Generate start */
#define I2C_CR1_STOP   0x0200   /* Generate stop */
#define I2C_CR1_ACK    0x0400   /* Acknowledge enable */

/* STM32F103 GPIO base addresses (MCP-verified) */
#define GPIOB_BASE  0x40010C00UL
#define GPIOB_CRL   (*(volatile uint32_t*)(GPIOB_BASE + 0x00))
#define GPIOB_CRH   (*(volatile uint32_t*)(GPIOB_BASE + 0x04))
#define GPIOB_ODR   (*(volatile uint32_t*)(GPIOB_BASE + 0x0C))

/* GPIO configuration for I2C pins (PB6=SCL, PB7=SDA):
 * CNF=11 (AF open-drain), MODE=11 (50MHz output) → 4-bit value 0xF
 * PB6 in CRL: bits 27:24 → set to 0xF
 * PB7 in CRL: bits 31:28 → set to 0xF
 */
#define I2C_SCL_PIN  6
#define I2C_SDA_PIN  7
#define I2C_GPIO_CFG 0xF  /* AF open-drain, 50MHz */

void i2c_init(void) {
    /* Configure PB6 (SCL) and PB7 (SDA) as AF open-drain */
    uint32_t crl = GPIOB_CRL;
    /* PB6: clear bits 27:24 and set to 0xF */
    crl &= ~(0xFUL << (I2C_SCL_PIN * 4));
    crl |= (I2C_GPIO_CFG << (I2C_SCL_PIN * 4));
    /* PB7: clear bits 31:28 and set to 0xF */
    crl &= ~(0xFUL << (I2C_SDA_PIN * 4));
    crl |= (I2C_GPIO_CFG << (I2C_SDA_PIN * 4));
    GPIOB_CRL = crl;

    /* Reset I2C1 (MCP-verified: I2C1RST=bit21 in APB1RSTR @ RCC+0x10) */
    *(volatile uint32_t*)(0x40021000UL + 0x10) |= (1UL << 21);
    *(volatile uint32_t*)(0x40021000UL + 0x10) &= ~(1UL << 21);

    /* Configure I2C1 timing for 100kHz standard mode (APB1=36MHz) */
    /* FREQ = APB1_MHz = 36 */
    I2C1_CR2 = (APB1_FREQ / 1000000UL) & 0x3F;  /* FREQ[5:0] */

    /* CCR = FREQ × 10μs / 2 = 36 × 5 = 180 (standard mode, DUTY=0)
     * MCP-verified: CCR bits[0:11], DUTY bit14, F_S bit15 */
    I2C1_CCR = 180;  /* Standard mode, CCR=180 */

    /* TRISE = FREQ + 1 = 37 (standard mode)
     * MCP-verified: TRISE offset 0x20 */
    I2C1_TRISE = (APB1_FREQ / 1000000UL) + 1;

    /* Enable I2C1 peripheral (MCP-verified: PE=bit0 in CR1) */
    I2C1_CR1 = I2C_CR1_PE;
}

void i2c_start(void) {
    I2C1_CR1 |= I2C_CR1_START;
    while (!(I2C1_SR1 & I2C_SR1_SB)) {
        __asm__ volatile ("nop");
    }
}

void i2c_stop(void) {
    I2C1_CR1 |= I2C_CR1_STOP;
}

void i2c_write_addr(uint8_t addr, uint8_t rw) {
    I2C1_DR = addr | rw;
    while (!(I2C1_SR1 & I2C_SR1_ADDR)) {
        __asm__ volatile ("nop");
    }
    (void)I2C1_SR2;  /* Clear ADDR flag by reading SR1 then SR2 */
}

void i2c_write_byte(uint8_t data) {
    while (!(I2C1_SR1 & I2C_SR1_TxE)) {
        __asm__ volatile ("nop");
    }
    I2C1_DR = data;
    while (!(I2C1_SR1 & I2C_SR1_BTF)) {
        __asm__ volatile ("nop");
    }
}

uint8_t i2c_read_byte(uint8_t ack) {
    if (ack) {
        I2C1_CR1 |= I2C_CR1_ACK;
    } else {
        I2C1_CR1 &= ~I2C_CR1_ACK;
    }
    while (!(I2C1_SR1 & I2C_SR1_RxNE)) {
        __asm__ volatile ("nop");
    }
    return (uint8_t)(I2C1_DR & 0xFF);
}

uint8_t i2c_read_nack(void) {
    return i2c_read_byte(0);
}

int i2c_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data) {
    i2c_start();
    i2c_write_addr(dev_addr, 0);  /* Write */
    i2c_write_byte(reg);
    i2c_write_byte(data);
    i2c_stop();
    return 0;
}

int i2c_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data) {
    /* Write register address */
    i2c_start();
    i2c_write_addr(dev_addr, 0);  /* Write */
    i2c_write_byte(reg);
    /* Repeated start for read */
    i2c_start();
    i2c_write_addr(dev_addr, 1);  /* Read */
    *data = i2c_read_nack();
    i2c_stop();
    return 0;
}

int i2c_read_regs(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint8_t len) {
    /* Write register address */
    i2c_start();
    i2c_write_addr(dev_addr, 0);  /* Write */
    i2c_write_byte(reg);
    /* Repeated start for read */
    i2c_start();
    i2c_write_addr(dev_addr, 1);  /* Read */
    for (uint8_t i = 0; i < len; i++) {
        if (i == len - 1) {
            data[i] = i2c_read_nack();
        } else {
            data[i] = i2c_read_byte(1);
        }
    }
    i2c_stop();
    return 0;
}
