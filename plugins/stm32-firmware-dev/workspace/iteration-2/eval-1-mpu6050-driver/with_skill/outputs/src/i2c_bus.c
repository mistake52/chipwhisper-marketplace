/**
 * i2c_bus.c — I2C1 bus driver, 100kHz standard mode
 *
 * MCP-verified register layout (RM0008 Rev 21, sec 26.6):
 *   I2C1_CR1:   0x40005400+0x00, PE=bit0, START=bit8, STOP=bit9, ACK=bit10
 *   I2C1_CR2:   0x40005400+0x04, FREQ=[0:5]
 *   I2C1_OAR1:  0x40005400+0x08
 *   I2C1_DR:    0x40005400+0x10, DR=[0:7]
 *   I2C1_SR1:   0x40005400+0x14, SB=bit0, ADDR=bit1, TXE=bit7, RxNE=bit6, BTF=bit2
 *   I2C1_SR2:   0x40005400+0x18, BUSY=bit1, TRA=bit2, MSL=bit0
 *   I2C1_CCR:   0x40005400+0x1C, CCR=[0:11], DUTY=bit14, F_S=bit15
 *   I2C1_TRISE: 0x40005400+0x20
 *
 * Timing for APB1=36MHz, standard mode 100kHz:
 *   CR2.FREQ = 36
 *   CCR = APB1_MHz * 10μs / 2 = 36 * 10 / 2 = 180 (0xB4)
 *   TRISE = APB1_MHz + 1 = 36 + 1 = 37 (0x25)
 */

#include "i2c_bus.h"
#include "rcc.h"
#include "main.h"

/* --- I2C1 Register Definitions (MCP-verified) --- */
#define I2C1_BASE       0x40005400UL
#define I2C1_CR1        (*(volatile uint32_t*)(I2C1_BASE + 0x00))
#define I2C1_CR2        (*(volatile uint32_t*)(I2C1_BASE + 0x04))
#define I2C1_OAR1       (*(volatile uint32_t*)(I2C1_BASE + 0x08))
#define I2C1_OAR2       (*(volatile uint32_t*)(I2C1_BASE + 0x0C))
#define I2C1_DR         (*(volatile uint32_t*)(I2C1_BASE + 0x10))
#define I2C1_SR1        (*(volatile uint32_t*)(I2C1_BASE + 0x14))
#define I2C1_SR2        (*(volatile uint32_t*)(I2C1_BASE + 0x18))
#define I2C1_CCR        (*(volatile uint32_t*)(I2C1_BASE + 0x1C))
#define I2C1_TRISE      (*(volatile uint32_t*)(I2C1_BASE + 0x20))

/* I2C1_CR1 bit definitions */
#define I2C_CR1_PE      (1UL << 0)
#define I2C_CR1_START   (1UL << 8)
#define I2C_CR1_STOP    (1UL << 9)
#define I2C_CR1_ACK     (1UL << 10)
#define I2C_CR1_POS     (1UL << 11)

/* I2C1_SR1 bit definitions */
#define I2C_SR1_SB      (1UL << 0)   /* Start bit sent (master) */
#define I2C_SR1_ADDR    (1UL << 1)   /* Address sent/matched */
#define I2C_SR1_BTF     (1UL << 2)   /* Byte transfer finished */
#define I2C_SR1_RxNE    (1UL << 6)   /* Receive buffer not empty */
#define I2C_SR1_TxE     (1UL << 7)   /* Transmit buffer empty */

/* I2C1_SR2 bit definitions */
#define I2C_SR2_BUSY    (1UL << 1)   /* Bus busy */
#define I2C_SR2_TRA     (1UL << 2)   /* Transmitter/receiver */
#define I2C_SR2_MSL     (1UL << 0)   /* Master mode */

/* I2C1_CCR bit definitions */
#define I2C_CCR_FS      (1UL << 15)  /* Fast mode selection */
#define I2C_CCR_DUTY    (1UL << 14)  /* Fm duty cycle */

/* --- GPIOB Register Definitions --- */
#define GPIOB_BASE      0x40010C00UL
#define GPIOB_CRL       (*(volatile uint32_t*)(GPIOB_BASE + 0x00))
#define GPIOB_CRH       (*(volatile uint32_t*)(GPIOB_BASE + 0x04))

void i2c_init(void)
{
    /* 1. Enable GPIOB and I2C1 clocks */
    RCC_APB2ENR |= RCC_APB2ENR_IOPBEN;           /* GPIOB on APB2 */
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;           /* I2C1 on APB1 */

    /* 2. Configure PB6 (SCL) and PB7 (SDA) as alternate function open-drain, 50MHz */
    /*    PB6 = CRL bits [27:24]: CNF=11(AF_OD), MODE=11(50MHz) = 0xF */
    /*    PB7 = CRL bits [31:28]: CNF=11(AF_OD), MODE=11(50MHz) = 0xF */
    {
        uint32_t crl = GPIOB_CRL;
        crl &= ~((0xFUL << 24) | (0xFUL << 28));
        crl |=  ((0xFUL << 24) | (0xFUL << 28));
        GPIOB_CRL = crl;
    }

    /* 3. Reset I2C1 by setting SWRST in CR1, then clear it */
    I2C1_CR1 |= (1UL << 15);      /* SWRST */
    I2C1_CR1 &= ~(1UL << 15);     /* Clear SWRST */

    /* 4. Set I2C1 clock frequency in CR2: FREQ = APB1 in MHz = 36 */
    I2C1_CR2 = 36;                /* APB1 = 36MHz */

    /* 5. Configure CCR for standard mode 100kHz:
     *    CCR = FREQ * t_SCL / 2 = 36 * 10μs = 360/2 = 180
     *    No fast mode (F_S=0), standard mode only */
    I2C1_CCR = 180;

    /* 6. Set TRISE for standard mode: FREQ + 1 = 37 */
    /*    TRISE max is 63 (6-bit field). 37 fits. */
    I2C1_TRISE = 37;

    /* 7. Enable I2C1 peripheral */
    I2C1_CR1 |= I2C_CR1_PE;
}

/* Generate START condition */
void i2c_start(void)
{
    I2C1_CR1 |= I2C_CR1_START;
    while (!(I2C1_SR1 & I2C_SR1_SB));  /* Wait for SB: start bit sent */
}

/* Generate STOP condition */
void i2c_stop(void)
{
    I2C1_CR1 |= I2C_CR1_STOP;
    while (I2C1_SR2 & I2C_SR2_BUSY);   /* Wait until bus not busy (optional) */
}

/* Send 7-bit I2C address + R/W direction */
void i2c_write_addr(uint8_t addr, uint8_t dir)
{
    I2C1_DR = (uint8_t)((addr << 1) | (dir & 0x01));
    while (!(I2C1_SR1 & I2C_SR1_ADDR));  /* Wait for address sent */
    (void)I2C1_SR2;                      /* Read SR2 to clear ADDR flag */
}

/* Write one byte to I2C bus */
void i2c_write_byte(uint8_t data)
{
    while (!(I2C1_SR1 & I2C_SR1_TxE));  /* Wait for TxE */
    I2C1_DR = data;
    while (!(I2C1_SR1 & I2C_SR1_BTF));  /* Wait for byte transfer complete */
}

/* Read one byte from I2C bus, send ACK or NAK */
uint8_t i2c_read_byte(uint8_t ack)
{
    if (ack) {
        I2C1_CR1 |= I2C_CR1_ACK;   /* Set ACK for next byte */
    } else {
        I2C1_CR1 &= ~I2C_CR1_ACK;  /* NAK for last byte */
    }
    while (!(I2C1_SR1 & I2C_SR1_RxNE));  /* Wait for RxNE */
    return (uint8_t)I2C1_DR;
}

/* Read one register from I2C device */
uint8_t i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr)
{
    uint8_t data;

    /* Write phase: send register address */
    i2c_start();
    i2c_write_addr(dev_addr, 0);    /* Write */
    i2c_write_byte(reg_addr);       /* Register address */

    /* Read phase: read one byte with NAK */
    i2c_start();                    /* Repeated start */
    i2c_write_addr(dev_addr, 1);    /* Read */
    data = i2c_read_byte(0);        /* Read with NAK */
    i2c_stop();

    return data;
}

/* Write one byte to an I2C device register */
void i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    i2c_start();
    i2c_write_addr(dev_addr, 0);    /* Write */
    i2c_write_byte(reg_addr);       /* Register address */
    i2c_write_byte(data);           /* Data */
    i2c_stop();
}

/* Read multiple bytes from I2C device starting at reg_addr */
void i2c_read_multi(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len)
{
    /* Write phase: send register address */
    i2c_start();
    i2c_write_addr(dev_addr, 0);
    i2c_write_byte(reg_addr);

    /* Read phase: read len bytes */
    i2c_start();                               /* Repeated start */
    i2c_write_addr(dev_addr, 1);               /* Read mode */

    for (uint8_t i = 0; i < len; i++) {
        if (i == len - 1) {
            buf[i] = i2c_read_byte(0);          /* Last byte: NAK */
        } else {
            buf[i] = i2c_read_byte(1);          /* ACK for more bytes */
        }
    }
    i2c_stop();
}
