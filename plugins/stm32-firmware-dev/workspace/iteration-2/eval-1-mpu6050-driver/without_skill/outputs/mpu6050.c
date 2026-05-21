/**
 * @file    mpu6050.c
 * @brief   MPU6050 I2C driver implementation for STM32F103C8T6.
 *
 * Uses I2C1 peripheral with 100 kHz standard-mode timing.
 * All register accesses are bare-metal — no HAL or CMSIS device headers.
 *
 * Dependencies provided by main.c / system layer:
 *   void delay_ms(uint32_t ms);
 */

#include "mpu6050.h"

/*=========================================================================*/
/*  Memory-Mapped Peripheral Register Definitions                          */
/*=========================================================================*/

/* --- RCC (Reset and Clock Control) ------------------------------------ */
#define RCC_BASE            0x40021000UL
#define RCC_APB1ENR         (*(volatile uint32_t *)(RCC_BASE + 0x1C))
#define RCC_APB1ENR_I2C1EN   (1UL << 21)

/* --- I2C1 -------------------------------------------------------------- */
typedef struct {
    volatile uint32_t CR1;    /* 0x00 */
    volatile uint32_t CR2;    /* 0x04 */
    volatile uint32_t OAR1;   /* 0x08 */
    volatile uint32_t OAR2;   /* 0x0C */
    volatile uint32_t DR;     /* 0x10 */
    volatile uint32_t SR1;    /* 0x14 */
    volatile uint32_t SR2;    /* 0x18 */
    volatile uint32_t CCR;    /* 0x1C */
    volatile uint32_t TRISE;  /* 0x20 */
} I2C_TypeDef;

#define I2C1  ((I2C_TypeDef *)0x40005400UL)

/* I2C_CR1 bits */
#define I2C_CR1_PE      (1UL << 0)
#define I2C_CR1_START   (1UL << 8)
#define I2C_CR1_STOP    (1UL << 9)
#define I2C_CR1_ACK     (1UL << 10)
#define I2C_CR1_POS     (1UL << 11)
#define I2C_CR1_SWRST   (1UL << 15)

/* I2C_SR1 bits */
#define I2C_SR1_SB      (1UL << 0)
#define I2C_SR1_ADDR    (1UL << 1)
#define I2C_SR1_BTF     (1UL << 2)
#define I2C_SR1_RXNE    (1UL << 6)
#define I2C_SR1_TXE     (1UL << 7)

/* I2C_SR2 bit */
#define I2C_SR2_BUSY    (1UL << 1)


/*=========================================================================*/
/*  External dependency: millisecond delay (provided by main.c)            */
/*=========================================================================*/
extern void delay_ms(uint32_t ms);


/*=========================================================================*/
/*  Private I2C helpers                                                    */
/*=========================================================================*/

/** Wait for I2C busy flag to clear. */
static void i2c_wait_busy(void)
{
    while (I2C1->SR2 & I2C_SR2_BUSY) {}
}

/** Wait for a specific SR1 flag with timeout. */
static int i2c_wait_sr1(uint32_t flag)
{
    uint32_t timeout = 100000;
    while (!(I2C1->SR1 & flag)) {
        if (--timeout == 0) return -1;
    }
    return 0;
}


/*=========================================================================*/
/*  Public API                                                             */
/*=========================================================================*/

/**
 * @brief  Initialize I2C1 for MPU6050 communication.
 *         Configures I2C1 at 100 kHz standard mode.
 *         PB6 (SCL) and PB7 (SDA) must already be configured as
 *         alternate-function open-drain by the GPIO init code.
 */
void mpu6050_i2c_init(void)
{
    /* Enable I2C1 clock (APB1) */
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* Software reset */
    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    /* Peripheral clock frequency = 36 MHz (APB1 = SYSCLK/2 = 72/2) */
    I2C1->CR2 = 36;

    /* Standard mode 100 kHz:
     *   CCR = f_PCLK / (2 * f_SCL) = 36e6 / 200e3 = 180
     */
    I2C1->CCR = 180;

    /* TRISE = (max_rise_time / T_PCLK1) + 1
     *   SM max rise = 1000 ns, T_PCLK1 = 1/36e6 = 27.78 ns
     *   TRISE = 1000 / 27.78 + 1 = 37
     */
    I2C1->TRISE = 37;

    /* Enable the I2C peripheral */
    I2C1->CR1 |= I2C_CR1_PE;
    i2c_wait_busy();
}

/**
 * @brief  Write one byte to an MPU6050 register.
 */
void mpu6050_write_reg(uint8_t reg, uint8_t data)
{
    /* START */
    I2C1->CR1 |= I2C_CR1_START;
    i2c_wait_sr1(I2C_SR1_SB);

    /* Slave address + write */
    I2C1->DR = (uint8_t)(MPU6050_ADDR << 1);
    i2c_wait_sr1(I2C_SR1_ADDR);
    (void)I2C1->SR2;  /* clear ADDR */

    /* Register address */
    I2C1->DR = reg;
    i2c_wait_sr1(I2C_SR1_TXE);

    /* Data byte */
    I2C1->DR = data;
    i2c_wait_sr1(I2C_SR1_TXE);
    i2c_wait_sr1(I2C_SR1_BTF);

    /* STOP */
    I2C1->CR1 |= I2C_CR1_STOP;
    i2c_wait_busy();
}

/**
 * @brief  Read one byte from an MPU6050 register.
 */
uint8_t mpu6050_read_reg(uint8_t reg)
{
    uint8_t val;

    /* START + slave address write */
    I2C1->CR1 |= I2C_CR1_START;
    i2c_wait_sr1(I2C_SR1_SB);

    I2C1->DR = (uint8_t)(MPU6050_ADDR << 1);
    i2c_wait_sr1(I2C_SR1_ADDR);
    (void)I2C1->SR2;

    /* Register address */
    I2C1->DR = reg;
    i2c_wait_sr1(I2C_SR1_TXE);
    i2c_wait_sr1(I2C_SR1_BTF);

    /* Repeated START + slave address read */
    I2C1->CR1 |= I2C_CR1_START;
    i2c_wait_sr1(I2C_SR1_SB);

    I2C1->DR = (uint8_t)((MPU6050_ADDR << 1) | 1);
    i2c_wait_sr1(I2C_SR1_ADDR);

    /* NACK + STOP for single byte */
    I2C1->CR1 &= ~I2C_CR1_ACK;
    (void)I2C1->SR2;
    I2C1->CR1 |= I2C_CR1_STOP;

    /* Read data */
    i2c_wait_sr1(I2C_SR1_RXNE);
    val = (uint8_t)I2C1->DR;

    /* Re-enable ACK */
    I2C1->CR1 |= I2C_CR1_ACK;
    i2c_wait_busy();

    return val;
}

/**
 * @brief  Read multiple consecutive bytes starting from a register.
 */
void mpu6050_read_burst(uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (len == 0) return;

    /* START + slave address write */
    I2C1->CR1 |= I2C_CR1_START;
    i2c_wait_sr1(I2C_SR1_SB);

    I2C1->DR = (uint8_t)(MPU6050_ADDR << 1);
    i2c_wait_sr1(I2C_SR1_ADDR);
    (void)I2C1->SR2;

    /* Register address */
    I2C1->DR = reg;
    i2c_wait_sr1(I2C_SR1_TXE);
    i2c_wait_sr1(I2C_SR1_BTF);

    /* Repeated START + slave address read */
    I2C1->CR1 |= I2C_CR1_START;
    i2c_wait_sr1(I2C_SR1_SB);

    I2C1->DR = (uint8_t)((MPU6050_ADDR << 1) | 1);
    i2c_wait_sr1(I2C_SR1_ADDR);

    if (len == 1) {
        /* Single byte: NACK + STOP before reading */
        I2C1->CR1 &= ~I2C_CR1_ACK;
        (void)I2C1->SR2;
        I2C1->CR1 |= I2C_CR1_STOP;
        i2c_wait_sr1(I2C_SR1_RXNE);
        buf[0] = (uint8_t)I2C1->DR;
    } else if (len == 2) {
        /* Two bytes: set POS + NACK for second byte */
        I2C1->CR1 |= I2C_CR1_POS;
        (void)I2C1->SR2;
        I2C1->CR1 &= ~I2C_CR1_ACK;
        i2c_wait_sr1(I2C_SR1_BTF);
        I2C1->CR1 |= I2C_CR1_STOP;
        buf[0] = (uint8_t)I2C1->DR;
        buf[1] = (uint8_t)I2C1->DR;
        I2C1->CR1 &= ~I2C_CR1_POS;
    } else {
        /* Three or more bytes */
        (void)I2C1->SR2;
        uint8_t i;
        for (i = 0; i < len; i++) {
            if (i == len - 3) {
                i2c_wait_sr1(I2C_SR1_BTF);
                I2C1->CR1 &= ~I2C_CR1_ACK;
                buf[i] = (uint8_t)I2C1->DR;
            } else if (i == len - 2) {
                i2c_wait_sr1(I2C_SR1_BTF);
                I2C1->CR1 |= I2C_CR1_STOP;
                buf[i] = (uint8_t)I2C1->DR;
                buf[i + 1] = (uint8_t)I2C1->DR;
                break;
            } else {
                i2c_wait_sr1(I2C_SR1_RXNE);
                buf[i] = (uint8_t)I2C1->DR;
            }
        }
    }

    /* Re-enable ACK */
    I2C1->CR1 |= I2C_CR1_ACK;
    i2c_wait_busy();
}


/*=========================================================================*/
/*  MPU6050 Sensor-Specific Functions                                      */
/*=========================================================================*/

/**
 * @brief  Initialize MPU6050: wake up, set sensor ranges and filters.
 * @return 0 on success, -1 if WHO_AM_I mismatches.
 */
int mpu6050_init(void)
{
    /* Check chip identity */
    if (mpu6050_read_reg(MPU6050_REG_WHO_AM_I) != 0x68) {
        return -1;
    }

    /* Wake up: clear sleep, use X-axis gyro PLL as clock */
    mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, MPU6050_PWR_MGMT1_CLKSEL_PLL_X);

    /* Wait for clock to stabilise */
    delay_ms(100);

    /* Sample rate = 1 kHz / (1 + SMPLRT_DIV) — we leave divider at 0 */
    mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, 0x00);

    /* DLPF = 5 Hz (max smoothing, lowest noise) */
    mpu6050_write_reg(MPU6050_REG_CONFIG, MPU6050_DLPF_5HZ);

    /* Gyro: +/- 250 deg/s */
    mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, MPU6050_GYRO_FS_250);

    /* Accel: +/- 2 g */
    mpu6050_write_reg(MPU6050_REG_ACCEL_CONFIG, MPU6050_ACCEL_FS_2G);

    return 0;
}

/**
 * @brief  Read all 6 sensor axes in a single burst.
 *         Data is big-endian from the sensor; returned as host-endian int16_t.
 */
void mpu6050_read_all(int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[14];

    mpu6050_read_burst(MPU6050_REG_ACCEL_XOUT_H, buf, 14);

    /* MPU6050 stores data big-endian */
    *ax = (int16_t)((buf[0]  << 8) | buf[1]);
    *ay = (int16_t)((buf[2]  << 8) | buf[3]);
    *az = (int16_t)((buf[4]  << 8) | buf[5]);
    /* buf[6..7] = temperature — not used here */
    *gx = (int16_t)((buf[8]  << 8) | buf[9]);
    *gy = (int16_t)((buf[10] << 8) | buf[11]);
    *gz = (int16_t)((buf[12] << 8) | buf[13]);
}
