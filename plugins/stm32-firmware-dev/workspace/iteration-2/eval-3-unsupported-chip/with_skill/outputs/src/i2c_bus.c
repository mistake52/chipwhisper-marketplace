/**
 * i2c_bus.c — I2C1 driver for STM32F407VGT6
 *
 * Register values from general knowledge — NOT MCP-verified.
 * I2C1 on APB1 @ 42MHz, standard mode (100kHz).
 * PB6=SCL, PB7=SDA, Alternate Function 4.
 *
 * Key F4 vs F1 difference: GPIO uses MODER/OTYPER/OSPEEDR/PUPDR/AFR registers
 * instead of the F1 CRL/CRH (CNF+MODE) scheme. GPIOC is on AHB1ENR on F4.
 */

#include "main.h"
#include "rcc.h"

/* ================================================================
 * GPIO Register Definitions (STM32F4 memory map)
 * ================================================================ */
#define GPIO_MODER(port)    (*(volatile uint32_t*)((port) + 0x00))
#define GPIO_OTYPER(port)   (*(volatile uint32_t*)((port) + 0x04))
#define GPIO_OSPEEDR(port)  (*(volatile uint32_t*)((port) + 0x08))
#define GPIO_PUPDR(port)    (*(volatile uint32_t*)((port) + 0x0C))
#define GPIO_AFRL(port)     (*(volatile uint32_t*)((port) + 0x20))
#define GPIO_AFRH(port)     (*(volatile uint32_t*)((port) + 0x24))

#define MODER_INPUT     0x0U
#define MODER_OUTPUT    0x1U
#define MODER_AF        0x2U
#define MODER_ANALOG    0x3U

/* ================================================================
 * I2C1 Register Definitions (base 0x40005400)
 * ================================================================ */
#define I2C_CR1     (*(volatile uint32_t*)(I2C1_BASE + 0x00))
#define I2C_CR2     (*(volatile uint32_t*)(I2C1_BASE + 0x04))
#define I2C_OAR1    (*(volatile uint32_t*)(I2C1_BASE + 0x08))
#define I2C_DR      (*(volatile uint32_t*)(I2C1_BASE + 0x10))
#define I2C_SR1     (*(volatile uint32_t*)(I2C1_BASE + 0x14))
#define I2C_SR2     (*(volatile uint32_t*)(I2C1_BASE + 0x18))
#define I2C_CCR     (*(volatile uint32_t*)(I2C1_BASE + 0x1C))
#define I2C_TRISE   (*(volatile uint32_t*)(I2C1_BASE + 0x20))

/* I2C_CR1 bits */
#define I2C_CR1_PE      (1U << 0)
#define I2C_CR1_START   (1U << 8)
#define I2C_CR1_STOP    (1U << 9)
#define I2C_CR1_ACK     (1U << 10)
#define I2C_CR1_SWRST   (1U << 15)

/* I2C_CR2 bits */
#define I2C_CR2_FREQ_Pos    0

/* I2C_SR1 bits */
#define I2C_SR1_SB      (1U << 0)
#define I2C_SR1_ADDR    (1U << 1)
#define I2C_SR1_BTF     (1U << 2)
#define I2C_SR1_TXE     (1U << 7)
#define I2C_SR1_RXNE    (1U << 6)
#define I2C_SR1_STOPF   (1U << 4)

/* I2C_SR2 bits */
#define I2C_SR2_MSL     (1U << 0)
#define I2C_SR2_BUSY    (1U << 1)

/* ================================================================
 * Static helper: configure a GPIO pin for I2C alternate function
 * ================================================================ */
static void gpio_set_af(uint32_t port, int pin, int af)
{
    uint32_t moder = GPIO_MODER(port);
    uint32_t moder_mask = 0x3U << (pin * 2);
    moder &= ~moder_mask;
    moder |= (MODER_AF << (pin * 2));
    GPIO_MODER(port) = moder;

    /* Open-drain for I2C */
    GPIO_OTYPER(port) |= (1U << pin);

    /* High speed (100MHz) */
    uint32_t ospeedr = GPIO_OSPEEDR(port);
    ospeedr &= ~(0x3U << (pin * 2));
    ospeedr |= (0x3U << (pin * 2));  /* 11 = 100MHz */
    GPIO_OSPEEDR(port) = ospeedr;

    /* No pull-up/pull-down (external pull-ups required for I2C) */
    uint32_t pupdr = GPIO_PUPDR(port);
    pupdr &= ~(0x3U << (pin * 2));
    /* 00 = no pull-up/pull-down */
    GPIO_PUPDR(port) = pupdr;

    /* Set alternate function number */
    if (pin <= 7) {
        uint32_t afrl = GPIO_AFRL(port);
        afrl &= ~(0xFU << (pin * 4));
        afrl |= ((uint32_t)af << (pin * 4));
        GPIO_AFRL(port) = afrl;
    } else {
        uint32_t afrh = GPIO_AFRH(port);
        afrh &= ~(0xFU << ((pin - 8) * 4));
        afrh |= ((uint32_t)af << ((pin - 8) * 4));
        GPIO_AFRH(port) = afrh;
    }
}

/* ================================================================
 * Public API
 * ================================================================ */

void i2c_init(void)
{
    /* 1. Enable GPIOB and I2C1 clocks */
    rcc_enable_gpio();   /* GPIOA+B+C on AHB1 */
    rcc_enable_i2c1();   /* I2C1 on APB1 */

    /* 2. Configure PB6 (SCL) and PB7 (SDA) as alternate function */
    gpio_set_af(GPIOB_BASE, I2C1_SCL_PIN, I2C1_AF);
    gpio_set_af(GPIOB_BASE, I2C1_SDA_PIN, I2C1_AF);

    /* 3. Reset I2C1 peripheral */
    I2C_CR1 |= I2C_CR1_SWRST;
    I2C_CR1 &= ~I2C_CR1_SWRST;

    /* 4. Set peripheral clock frequency (APB1 MHz) */
    I2C_CR2 = ((PCLK1_FREQ / 1000000UL) << I2C_CR2_FREQ_Pos);

    /* 5. Configure clock control register for 100kHz standard mode */
    I2C_CCR = I2C1_CCR_VALUE;  /* CCR = 210 for 100kHz */

    /* 6. Set maximum rise time */
    I2C_TRISE = I2C1_TRISE_VALUE;  /* TRISE = 43 */

    /* 7. Enable I2C1 */
    I2C_CR1 |= I2C_CR1_PE;
}

/**
 * Send I2C start condition. Returns 0 on success, -1 on timeout.
 */
static int i2c_start(void)
{
    uint32_t timeout = 100000;

    /* Wait for bus idle */
    while ((I2C_SR2 & I2C_SR2_BUSY) && timeout) timeout--;
    if (!timeout) return -1;

    /* Generate start */
    I2C_CR1 |= I2C_CR1_START;
    timeout = 100000;
    while (!(I2C_SR1 & I2C_SR1_SB) && timeout) timeout--;
    if (!timeout) return -1;

    return 0;
}

/**
 * Send I2C stop condition.
 */
static void i2c_stop(void)
{
    I2C_CR1 |= I2C_CR1_STOP;
}

/**
 * Send one byte via I2C (address or data). Returns 0 on success.
 */
static int i2c_send_byte(uint8_t byte)
{
    uint32_t timeout = 100000;

    while (!(I2C_SR1 & I2C_SR1_TXE) && timeout) timeout--;
    if (!timeout) return -1;

    I2C_DR = byte;

    /* Wait for byte transfer complete */
    timeout = 100000;
    while (!(I2C_SR1 & (I2C_SR1_BTF | I2C_SR1_ADDR)) && timeout) timeout--;
    if (!timeout) return -1;

    return 0;
}

/**
 * Read one byte via I2C. Set last=1 to send NACK for the final byte.
 */
static uint8_t i2c_read_byte(int last)
{
    uint32_t timeout = 100000;

    if (last) {
        I2C_CR1 &= ~I2C_CR1_ACK;  /* NACK for last byte */
    } else {
        I2C_CR1 |= I2C_CR1_ACK;   /* ACK for intermediate bytes */
    }

    while (!(I2C_SR1 & I2C_SR1_RXNE) && timeout) timeout--;

    return (uint8_t)(I2C_DR & 0xFF);
}

/**
 * Check if an I2C device acknowledges its address. Returns 1 if present, 0 if not.
 */
int i2c_detect(uint8_t addr)
{
    int rc;

    rc = i2c_start();
    if (rc) return 0;

    rc = i2c_send_byte((uint8_t)((addr << 1) | 0));  /* Write address */
    i2c_stop();

    return (rc == 0) ? 1 : 0;
}

/**
 * Write to an I2C device register and read back data.
 *
 * Writes: [START][DEV_ADDR+W][REG_ADDR][RESTART][DEV_ADDR+R][DATA...][NACK][STOP]
 *
 * Returns 0 on success, negative on error.
 */
int i2c_write_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, int len)
{
    int rc;

    if (len <= 0) return -1;

    /* Send START + device address (write) */
    rc = i2c_start();
    if (rc) return -1;

    rc = i2c_send_byte((uint8_t)((dev_addr << 1) | 0));
    if (rc) { i2c_stop(); return -2; }

    /* Send register address */
    rc = i2c_send_byte(reg_addr);
    if (rc) { i2c_stop(); return -3; }

    /* Repeated START for read */
    rc = i2c_start();
    if (rc) return -4;

    rc = i2c_send_byte((uint8_t)((dev_addr << 1) | 1));
    if (rc) { i2c_stop(); return -5; }

    /* Read bytes: ACK all except last */
    for (int i = 0; i < len; i++) {
        data[i] = i2c_read_byte(i == (len - 1));
    }

    i2c_stop();
    return 0;
}
