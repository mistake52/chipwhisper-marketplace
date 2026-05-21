/**
 * @file    main.c
 * @brief   STM32F103C8T6 + MPU6050 bare-metal firmware.
 *
 * Reads accelerometer (g) and gyroscope (deg/s) data from MPU6050 via I2C1
 * and prints the values to USART1 (PA9 TX, 115200-8N1) once per second.
 *
 * Pin connections:
 *   MPU6050  VCC -> 3.3V
 *   MPU6050  GND -> GND
 *   MPU6050  SCL -> PB6
 *   MPU6050  SDA -> PB7
 *   USART1   TX  -> PA9
 *   USART1   RX  -> PA10 (optional)
 *   LED      PC13 (active low, on-board)
 *
 * Build:  arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 ... (see Makefile)
 */

#include <stdint.h>
#include "mpu6050.h"

/*=========================================================================*/
/*  External declarations (defined in startup / linker script)             */
/*=========================================================================*/
extern void Default_Handler(void);

/* Flash/RAM boundaries from linker */
extern uint32_t _estack;       /* Top of stack (from linker script) */

/*=========================================================================*/
/*  Forward declarations                                                    */
/*=========================================================================*/
void Reset_Handler(void);
void SysTick_Handler(void);

static void uart_putc(char c);
static void uart_print(const char *str);
static void uart_print_int(int32_t val);
static void uart_print_fixed(int32_t val, int32_t scale);

/*=========================================================================*/
/*  Vector Table                                                            */
/*  Section attribute places it at the start of .text (flash base)         */
/*=========================================================================*/
__attribute__((section(".isr_vector"), used))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))&_estack,      /*  0: Initial SP value          */
    Reset_Handler,                 /*  1: Reset                     */
    /* ... Cortex-M3 system exceptions ... */
    Default_Handler,               /*  2: NMI                       */
    Default_Handler,               /*  3: HardFault                 */
    Default_Handler,               /*  4: MemManage                 */
    Default_Handler,               /*  5: BusFault                  */
    Default_Handler,               /*  6: UsageFault                */
    0,                             /*  7: Reserved                  */
    0,                             /*  8: Reserved                  */
    0,                             /*  9: Reserved                  */
    0,                             /* 10: Reserved                  */
    Default_Handler,               /* 11: SVCall                    */
    Default_Handler,               /* 12: Debug Monitor             */
    0,                             /* 13: Reserved                  */
    Default_Handler,               /* 14: PendSV                    */
    SysTick_Handler,               /* 15: SysTick                   */
    /* ... STM32F103 IRQs start at offset 16 ... */
    Default_Handler,               /* 16: WWDG                      */
    Default_Handler,               /* 17: PVD                       */
    Default_Handler,               /* 18: TAMPER                    */
    Default_Handler,               /* 19: RTC                       */
    Default_Handler,               /* 20: FLASH                     */
    Default_Handler,               /* 21: RCC                       */
    Default_Handler,               /* 22: EXTI0                     */
    Default_Handler,               /* 23: EXTI1                     */
    Default_Handler,               /* 24: EXTI2                     */
    Default_Handler,               /* 25: EXTI3                     */
    Default_Handler,               /* 26: EXTI4                     */
    Default_Handler,               /* 27: DMA1_Channel1             */
    Default_Handler,               /* 28: DMA1_Channel2             */
    Default_Handler,               /* 29: DMA1_Channel3             */
    Default_Handler,               /* 30: DMA1_Channel4             */
    Default_Handler,               /* 31: DMA1_Channel5             */
    Default_Handler,               /* 32: DMA1_Channel6             */
    Default_Handler,               /* 33: DMA1_Channel7             */
    Default_Handler,               /* 34: ADC1_2                    */
    Default_Handler,               /* 35: USB_HP_CAN_TX             */
    Default_Handler,               /* 36: USB_LP_CAN_RX0            */
    Default_Handler,               /* 37: CAN_RX1                   */
    Default_Handler,               /* 38: CAN_SCE                   */
    Default_Handler,               /* 39: EXTI9_5                   */
    Default_Handler,               /* 40: TIM1_BRK                  */
    Default_Handler,               /* 41: TIM1_UP                   */
    Default_Handler,               /* 42: TIM1_TRG_COM              */
    Default_Handler,               /* 43: TIM1_CC                   */
    Default_Handler,               /* 44: TIM2                      */
    Default_Handler,               /* 45: TIM3                      */
    Default_Handler,               /* 46: TIM4                      */
    Default_Handler,               /* 47: I2C1_EV                   */
    Default_Handler,               /* 48: I2C1_ER                   */
    Default_Handler,               /* 49: I2C2_EV                   */
    Default_Handler,               /* 50: I2C2_ER                   */
    Default_Handler,               /* 51: SPI1                      */
    Default_Handler,               /* 52: SPI2                      */
    Default_Handler,               /* 53: USART1                    */
    Default_Handler,               /* 54: USART2                    */
    Default_Handler,               /* 55: USART3                    */
    Default_Handler,               /* 56: EXTI15_10                 */
    Default_Handler,               /* 57: RTCAlarm                  */
    Default_Handler,               /* 58: USBWakeup                 */
};

/*=========================================================================*/
/*  Default fault handler — infinite loop                                  */
/*=========================================================================*/
void Default_Handler(void)
{
    while (1) {}
}

/*=========================================================================*/
/*  Reset Handler — calls system init then branches to main()              */
/*=========================================================================*/
void Reset_Handler(void)
{
    /* Initialize .data section */
    extern uint32_t _sidata, _sdata, _edata;
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero-fill .bss section */
    extern uint32_t _sbss, _ebss;
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* Optional: call system init for FPU / cache (Cortex-M3 has neither) */
    /* Then jump to user main */
    extern int main(void);
    main();

    /* Should never return */
    while (1) {}
}


/*=========================================================================*/
/*  Peripheral Base Addresses & Registers (local to main)                  */
/*=========================================================================*/

#define RCC_BASE       0x40021000UL
#define RCC_CR         (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_CFGR       (*(volatile uint32_t *)(RCC_BASE + 0x04))
#define RCC_AHBENR     (*(volatile uint32_t *)(RCC_BASE + 0x14))
#define RCC_APB2ENR    (*(volatile uint32_t *)(RCC_BASE + 0x18))
#define RCC_APB1ENR    (*(volatile uint32_t *)(RCC_BASE + 0x1C))

#define FLASH_ACR      (*(volatile uint32_t *)(0x40022000UL))

/* GPIO */
typedef struct { volatile uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR; } GPIO_t;
#define GPIOA  ((GPIO_t *)0x40010800UL)
#define GPIOB  ((GPIO_t *)0x40010C00UL)
#define GPIOC  ((GPIO_t *)0x40011000UL)

/* USART1 */
typedef struct { volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR; } USART_t;
#define USART1 ((USART_t *)0x40013800UL)

/* SysTick */
typedef struct { volatile uint32_t CTRL, LOAD, VAL, CALIB; } SysTick_t;
#define SYSTICK ((SysTick_t *)0xE000E010UL)


/*=========================================================================*/
/*  Global tick counter                                                     */
/*=========================================================================*/
static volatile uint32_t g_ms;


/*=========================================================================*/
/*  SysTick Handler (1 ms tick)                                             */
/*=========================================================================*/
void SysTick_Handler(void)
{
    g_ms++;
}


/*=========================================================================*/
/*  Blocking delay (milliseconds)                                           */
/*=========================================================================*/
void delay_ms(uint32_t ms)
{
    uint32_t target = g_ms + ms;
    while (g_ms < target) { /* spin */ }
}


/*=========================================================================*/
/*  System clock: HSE (8 MHz) -> PLL x9 -> 72 MHz                          */
/*=========================================================================*/
static void clock_init(void)
{
    /* 2 wait states required for 48 < SYSCLK <= 72 MHz */
    FLASH_ACR = (2UL << 0);

    /* Enable HSE */
    RCC_CR |= (1UL << 16);
    while (!(RCC_CR & (1UL << 17))) {}

    /* PLL: HSE source, x9 multiplier */
    uint32_t cfgr = RCC_CFGR;
    cfgr &= ~(0xFUL << 18);        /* clear PLLMUL */
    cfgr |= (7UL << 18);           /* PLLMUL = x9     */
    cfgr |= (1UL << 16);           /* PLLSRC = HSE    */
    cfgr |= (4UL << 8);            /* PPRE1 = /2      */
    RCC_CFGR = cfgr;

    /* Enable PLL */
    RCC_CR |= (1UL << 24);
    while (!(RCC_CR & (1UL << 25))) {}

    /* Switch to PLL */
    RCC_CFGR = (RCC_CFGR & ~3UL) | 2UL;
    while ((RCC_CFGR & 0xCUL) != 8UL) {}
}


/*=========================================================================*/
/*  SysTick at 1 kHz (1 ms period)                                          */
/*=========================================================================*/
static void systick_init(void)
{
    /* CLKSOURCE = 0: AHB / 8 = 9 MHz
     * RELOAD = 9,000,000 / 1000 - 1 = 8999 */
    SYSTICK->LOAD = 8999;
    SYSTICK->VAL  = 0;
    SYSTICK->CTRL = (1UL << 0) | (1UL << 1);  /* ENABLE + TICKINT */
}


/*=========================================================================*/
/*  GPIO init: PC13 LED, PA9/PA10 USART1, PB6/PB7 I2C1                     */
/*=========================================================================*/
static void gpio_init(void)
{
    RCC_APB2ENR |= (1UL << 2) | (1UL << 3) | (1UL << 4);  /* IOPA/B/C */

    /* PC13: push-pull output, 2 MHz */
    GPIOC->CRH &= ~(0xFUL << 20);
    GPIOC->CRH |= (2UL << 20);

    /* PA9  (USART1 TX): alternate push-pull, 50 MHz */
    GPIOA->CRH &= ~(0xFUL << 4);
    GPIOA->CRH |= (0xBUL << 4);

    /* PA10 (USART1 RX): input floating */
    GPIOA->CRH &= ~(0xFUL << 8);
    GPIOA->CRH |= (4UL << 8);

    /* PB6 (I2C1 SCL): alternate open-drain, 50 MHz */
    GPIOB->CRL &= ~(0xFUL << 24);
    GPIOB->CRL |= (0xFUL << 24);

    /* PB7 (I2C1 SDA): alternate open-drain, 50 MHz */
    GPIOB->CRL &= ~(0xFUL << 28);
    GPIOB->CRL |= (0xFUL << 28);
}


/*=========================================================================*/
/*  USART1 init: 115200-8N1                                                 */
/*=========================================================================*/
static void usart1_init(void)
{
    RCC_APB2ENR |= (1UL << 14);   /* USART1 clock */

    /* BRR = 39.0625 -> mantissa 39, fraction 1 -> 0x0271 */
    USART1->BRR = 0x0271;
    USART1->CR1 = (1UL << 3) | (1UL << 2);  /* TE + RE */
    USART1->CR1 |= (1UL << 13);             /* UE       */
}


/*=========================================================================*/
/*  UART output helpers                                                     */
/*=========================================================================*/
static void uart_putc(char c)
{
    while (!(USART1->SR & (1UL << 7))) {}  /* wait TXE */
    USART1->DR = c;
}

static void uart_print(const char *s)
{
    while (*s) uart_putc(*s++);
}

static void uart_print_int(int32_t val)
{
    char buf[12];
    int i = 0;
    uint32_t u = (val < 0) ? (uint32_t)(-val) : (uint32_t)val;

    if (val < 0) uart_putc('-');
    if (u == 0) { uart_putc('0'); return; }
    while (u) { buf[i++] = '0' + (u % 10); u /= 10; }
    while (i)  uart_putc(buf[--i]);
}

/** Print val/scale with 2 decimal places (fixed-point). */
static void uart_print_fixed(int32_t val, int32_t scale)
{
    int32_t int_part, frac;

    if (val < 0) { uart_putc('-'); val = -val; }

    int_part = val / scale;
    frac = ((val % scale) * 100 + scale / 2) / scale;
    if (frac >= 100) { int_part++; frac = 0; }

    uart_print_int(int_part);
    uart_putc('.');
    if (frac < 10) uart_putc('0');
    uart_print_int(frac);
}


/*=========================================================================*/
/*  Main                                                                    */
/*=========================================================================*/
int main(void)
{
    int16_t ax_raw, ay_raw, az_raw;
    int16_t gx_raw, gy_raw, gz_raw;
    int32_t accel_x, accel_y, accel_z;  /* scaled fixed-point values */
    int32_t gyro_x,  gyro_y,  gyro_z;
    uint32_t iteration = 0;

    /* --- Hardware initialisation --- */
    clock_init();
    systick_init();
    gpio_init();
    usart1_init();

    /* PC13 LED on to show we're alive */
    GPIOC->BRR = (1UL << 13);  /* LED ON (active low) */

    uart_print("\r\n==========================================\r\n");
    uart_print("  STM32F103 + MPU6050 Bare-Metal Driver\r\n");
    uart_print("==========================================\r\n\r\n");

    /* Initialize I2C1 and MPU6050 */
    mpu6050_i2c_init();
    delay_ms(10);

    uart_print("MPU6050 init... ");
    if (mpu6050_init() != 0) {
        uart_print("FAILED (check wiring / I2C address)\r\n");
        /* Blink LED fast to signal error */
        while (1) {
            GPIOC->BSRR = (1UL << 13);  /* LED OFF */
            delay_ms(200);
            GPIOC->BRR  = (1UL << 13);  /* LED ON  */
            delay_ms(200);
        }
    }
    uart_print("OK\r\n\r\n");

    uart_print("Reading sensor data every 1 second...\r\n");
    uart_print("Format: iter | Accel(g) X Y Z | Gyro(dps) X Y Z\r\n");
    uart_print("--------------------------------------------------\r\n\r\n");

    /* --- Main loop: read + print every 1 second --- */
    while (1) {
        iteration++;

        /* Read raw sensor data (single burst read) */
        mpu6050_read_all(&ax_raw, &ay_raw, &az_raw,
                         &gx_raw, &gy_raw, &gz_raw);

        /* Convert to physical units using fixed-point (scale by 100):
         *   accel_g  = raw * 100 / 16384  (accel in cg, centi-g)
         *   gyro_dps = raw * 100 / 131    (gyro in cdps, centi-deg/s)
         */
        accel_x = (int32_t)ax_raw * 100 / 16384;
        accel_y = (int32_t)ay_raw * 100 / 16384;
        accel_z = (int32_t)az_raw * 100 / 16384;

        gyro_x  = (int32_t)gx_raw * 100 / 131;
        gyro_y  = (int32_t)gy_raw * 100 / 131;
        gyro_z  = (int32_t)gz_raw * 100 / 131;

        /* Print iteration counter */
        uart_print("[");
        uart_print_int((int32_t)iteration);
        uart_print("] ");

        /* Print accelerometer */
        uart_print("Accel(g): X=");
        uart_print_fixed(accel_x, 100);
        uart_print(" Y=");
        uart_print_fixed(accel_y, 100);
        uart_print(" Z=");
        uart_print_fixed(accel_z, 100);

        /* Print gyroscope */
        uart_print("  Gyro(dps): X=");
        uart_print_fixed(gyro_x, 100);
        uart_print(" Y=");
        uart_print_fixed(gyro_y, 100);
        uart_print(" Z=");
        uart_print_fixed(gyro_z, 100);
        uart_print("\r\n");

        /* Toggle LED */
        GPIOC->ODR ^= (1UL << 13);

        /* Wait 1 second */
        delay_ms(1000);
    }

    return 0;
}
