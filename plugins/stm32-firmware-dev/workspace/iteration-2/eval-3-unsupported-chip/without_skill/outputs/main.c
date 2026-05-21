/**
 * @file    main.c
 * @brief   STM32F407VGT6 Discovery Board: LM75 I2C Temperature Sensor + LCD
 *
 * Hardware connections:
 *   LM75 (I2C1):
 *     SCL : PB6  (I2C1_SCL, AF4)
 *     SDA : PB9  (I2C1_SDA, AF4)
 *     VCC : 3.3V
 *     GND : GND
 *     A0-A2: GND (address 0x48)
 *
 *   ILI9341 LCD (SPI2):
 *     SCK  : PB13 (SPI2_SCK,  AF5)
 *     MOSI : PB15 (SPI2_MOSI, AF5)
 *     CS   : PE0  (GPIO Output)
 *     DC   : PE1  (GPIO Output)
 *     RST  : PE2  (GPIO Output)
 *     VCC  : 3.3V
 *     GND  : GND
 *     LED  : 3.3V (backlight)
 *
 *   On-board LEDs:
 *     LD3 (Orange): PD13
 *     LD4 (Green):  PD12
 *     LD5 (Red):    PD14
 *     LD6 (Blue):   PD15
 *
 * System clock: 168 MHz (HSE 8 MHz -> PLL)
 *   HCLK  = 168 MHz
 *   APB1  = 42 MHz
 *   APB2  = 84 MHz
 *
 * This application reads the LM75 temperature every second and displays
 * it on the ILI9341 LCD. The orange LED toggles on each reading to show
 * the application is alive.
 */

/* --------------------------------------------------------------------------
 * Includes
 * -------------------------------------------------------------------------- */
#include "stm32f4xx_hal.h"
#include "lm75.h"
#include "lcd_ili9341.h"
#include <stdio.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Private defines
 * -------------------------------------------------------------------------- */

/* I2C1 configuration for LM75 */
#define LM75_I2C_TIMING         0x40912732U  /* 100 kHz with 42 MHz APB1 clock */
#define LM75_I2C_ADDRESS        0x48U

/* SPI2 configuration for ILI9341 LCD */
#define LCD_SPI_BAUDRATE_PRESCALER  SPI_BAUDRATEPRESCALER_4  /* 84/4 = 21 MHz */

/* LED pins */
#define LED_ORANGE_PIN          GPIO_PIN_13
#define LED_GREEN_PIN           GPIO_PIN_12
#define LED_RED_PIN             GPIO_PIN_14
#define LED_BLUE_PIN            GPIO_PIN_15
#define LED_PORT                GPIOD

/* I2C1 pins */
#define I2C1_SCL_PIN            GPIO_PIN_6
#define I2C1_SDA_PIN            GPIO_PIN_9
#define I2C1_PORT               GPIOB
#define I2C1_SCL_AF             GPIO_AF4_I2C1
#define I2C1_SDA_AF             GPIO_AF4_I2C1

/* SPI2 pins */
#define SPI2_SCK_PIN            GPIO_PIN_13
#define SPI2_MOSI_PIN           GPIO_PIN_15
#define SPI2_PORT               GPIOB
#define SPI2_AF                 GPIO_AF5_SPI2

/* LCD control pins (GPIOE) */
#define LCD_CS_PIN              GPIO_PIN_0
#define LCD_DC_PIN              GPIO_PIN_1
#define LCD_RST_PIN             GPIO_PIN_2
#define LCD_CTRL_PORT           GPIOE

/* Temperature read interval in milliseconds */
#define TEMP_READ_INTERVAL_MS   1000U

/* LCD display layout constants */
#define LCD_BG_COLOR            COLOR_BLACK
#define LCD_TITLE_COLOR         COLOR_CYAN
#define LCD_TEMP_COLOR          COLOR_YELLOW
#define LCD_VALUE_COLOR         COLOR_WHITE
#define LCD_STATUS_COLOR        COLOR_GREEN
#define LCD_ERROR_COLOR         COLOR_RED
#define LCD_HEADER_BG           COLOR_NAVY

/* --------------------------------------------------------------------------
 * Private variables
 * -------------------------------------------------------------------------- */
static I2C_HandleTypeDef  hi2c1;
static SPI_HandleTypeDef  hspi2;

/* --------------------------------------------------------------------------
 * Private function prototypes
 * -------------------------------------------------------------------------- */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI2_Init(void);
static void DisplayError(const char *message);
static void DisplayTemperature(float temperature);
static void DrawHeader(void);
static void UpdateStatusBar(const char *status);
static void Error_Handler(void);

/* --------------------------------------------------------------------------
 * System Clock Configuration
 * HSE: 8 MHz (from ST-LINK MCO)
 * PLL: HSE / 8 = 1 MHz, N = 336, P = 2 -> 168 MHz
 * HCLK = 168 MHz, APB1 = 42 MHz (max), APB2 = 84 MHz (max)
 * -------------------------------------------------------------------------- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Enable Power Control clock */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* Configure voltage regulator scale 1 for 168 MHz */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSE oscillator configuration: 8 MHz external */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 8;    /* HSE/8 = 1 MHz input to PLL */
    RCC_OscInitStruct.PLL.PLLN       = 336;  /* VCO = 1 MHz * 336 = 336 MHz */
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;  /* PLL output = 336/2 = 168 MHz */
    RCC_OscInitStruct.PLL.PLLQ       = 4;    /* 336/4 = 84 MHz for USB OTG FS */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* Clock tree configuration */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK  |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1  |
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK = 168 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;     /* PCLK1 = 42 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;     /* PCLK2 = 84 MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    /* Configure SysTick to generate 1ms interrupts */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

/* --------------------------------------------------------------------------
 * GPIO Initialization
 * -------------------------------------------------------------------------- */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO clocks */
    __HAL_RCC_GPIOD_CLK_ENABLE();  /* LEDs */
    __HAL_RCC_GPIOB_CLK_ENABLE();  /* I2C1, SPI2 */
    __HAL_RCC_GPIOE_CLK_ENABLE();  /* LCD control pins */

    /* ---- On-board LEDs (GPIOD) ---- */
    GPIO_InitStruct.Pin   = LED_ORANGE_PIN | LED_GREEN_PIN |
                           LED_RED_PIN | LED_BLUE_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

    /* Turn all LEDs off initially */
    HAL_GPIO_WritePin(LED_PORT,
                      LED_ORANGE_PIN | LED_GREEN_PIN | LED_RED_PIN | LED_BLUE_PIN,
                      GPIO_PIN_RESET);

    /* ---- LCD control pins (GPIOE) ---- */
    GPIO_InitStruct.Pin   = LCD_CS_PIN | LCD_DC_PIN | LCD_RST_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_CTRL_PORT, &GPIO_InitStruct);

    /* CS and RST high initially; DC low */
    HAL_GPIO_WritePin(LCD_CTRL_PORT, LCD_CS_PIN | LCD_RST_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CTRL_PORT, LCD_DC_PIN, GPIO_PIN_RESET);
}

/* --------------------------------------------------------------------------
 * I2C1 Initialization (for LM75)
 * -------------------------------------------------------------------------- */
static void MX_I2C1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable I2C1 clock */
    __HAL_RCC_I2C1_CLK_ENABLE();

    /* ---- I2C1 GPIO: PB6 (SCL), PB9 (SDA) ---- */
    GPIO_InitStruct.Pin       = I2C1_SCL_PIN | I2C1_SDA_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = I2C1_SCL_AF;  /* Both pins use AF4 on PB port */
    HAL_GPIO_Init(I2C1_PORT, &GPIO_InitStruct);

    /* ---- I2C1 peripheral ---- */
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;   /* 100 kHz standard mode */
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0x00;     /* Not used (we're master) */
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }

    /* Configure analog filter and digital noise filter */
    /* Enable analog filter; set digital noise filter to 0 */
    HAL_I2CEx_AnalogFilter_Config(&hi2c1, I2C_ANALOGFILTER_ENABLE);
    HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0);
}

/* --------------------------------------------------------------------------
 * SPI2 Initialization (for ILI9341 LCD)
 * -------------------------------------------------------------------------- */
static void MX_SPI2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable SPI2 clock */
    __HAL_RCC_SPI2_CLK_ENABLE();

    /* ---- SPI2 GPIO: PB13 (SCK), PB15 (MOSI) ---- */
    GPIO_InitStruct.Pin       = SPI2_SCK_PIN | SPI2_MOSI_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = SPI2_AF;
    HAL_GPIO_Init(SPI2_PORT, &GPIO_InitStruct);

    /* ---- SPI2 peripheral ---- */
    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_1LINE;  /* Transmit only (no MISO) */
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi2.Init.NSS               = SPI_NSS_SOFT;         /* Software CS */
    hspi2.Init.BaudRatePrescaler = LCD_SPI_BAUDRATE_PRESCALER;
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial     = 7;

    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
        Error_Handler();
    }
}

/* --------------------------------------------------------------------------
 * Display helpers
 * -------------------------------------------------------------------------- */
static void DrawHeader(void)
{
    /* Draw a dark blue header bar at the top */
    LCD_FillRect(0, 0, ILI9341_WIDTH, 40, LCD_HEADER_BG);

    /* Title text centered in the header */
    const char *title = "LM75 Temperature Monitor";
    uint16_t title_len = strlen(title);
    uint16_t char_w = LCD_CharWidth(FONT_SMALL);
    uint16_t title_w = title_len * char_w;
    uint16_t title_x = (ILI9341_WIDTH - title_w) / 2;

    LCD_DrawString(title_x, 5, title, LCD_TITLE_COLOR, LCD_HEADER_BG, FONT_SMALL);

    /* Horizontal divider line */
    LCD_DrawHLine(0, 40, ILI9341_WIDTH, COLOR_GRAY);
}

static void UpdateStatusBar(const char *status)
{
    /* Status bar at the bottom of the screen */
    uint16_t y = ILI9341_HEIGHT - 20;
    LCD_FillRect(0, y, ILI9341_WIDTH, 20, COLOR_DARK_GRAY);

    uint16_t char_w = LCD_CharWidth(FONT_SMALL);
    uint16_t status_w = strlen(status) * char_w;
    uint16_t status_x = (ILI9341_WIDTH - status_w) / 2;

    LCD_DrawString(status_x, y + 2, status, COLOR_LIGHT_BLUE, COLOR_DARK_GRAY, FONT_SMALL);
}

static void DisplayTemperature(float temperature)
{
    char temp_str[32];
    int16_t temp_whole;
    uint16_t temp_fraction;

    /* Split into whole and fractional part (1 decimal) */
    temp_whole    = (int16_t)temperature;
    temp_fraction = (uint16_t)((temperature - (float)temp_whole) * 10.0f);
    if (temp_fraction < 0) temp_fraction = (uint16_t)(-temperature + (float)(-temp_whole)) * 10;
    /* Simple unsigned fix: use absolute value for fraction */
    if (temperature < 0 && temp_whole == 0) {
        temp_fraction = (uint16_t)((-temperature) * 10.0f);
    } else if (temperature >= 0) {
        temp_fraction = (uint16_t)((temperature - (float)temp_whole) * 10.0f + 0.5f);
    } else {
        temp_fraction = (uint16_t)((-temperature + (float)temp_whole) * 10.0f + 0.5f);
    }
    if (temp_fraction >= 10) {
        temp_fraction = 0;
        if (temperature >= 0) temp_whole++;
        else temp_whole--;
    }

    /* Clear the display area (between header and status bar) */
    LCD_FillRect(0, 42, ILI9341_WIDTH, ILI9341_HEIGHT - 62, LCD_BG_COLOR);

    /* Draw a decorative box around the temperature */
    uint16_t box_x = 40;
    uint16_t box_y = 80;
    uint16_t box_w = ILI9341_WIDTH - 80;
    uint16_t box_h = 140;

    /* Box border */
    LCD_DrawHLine(box_x, box_y, box_w, COLOR_GRAY);
    LCD_DrawHLine(box_x, box_y + box_h, box_w, COLOR_GRAY);
    LCD_DrawVLine(box_x, box_y, box_h, COLOR_GRAY);
    LCD_DrawVLine(box_x + box_w, box_y, box_h, COLOR_GRAY);

    /* "TEMPERATURE" label inside the box */
    const char *label = "TEMPERATURE";
    uint16_t label_w = strlen(label) * LCD_CharWidth(FONT_SMALL);
    LCD_DrawString((ILI9341_WIDTH - label_w) / 2, box_y + 15,
                   label, COLOR_CYAN, LCD_BG_COLOR, FONT_SMALL);

    /* Temperature value - large font, centered */
    snprintf(temp_str, sizeof(temp_str), "%+d.%01u C", temp_whole, temp_fraction);
    uint16_t temp_w = strlen(temp_str) * LCD_CharWidth(FONT_LARGE);
    LCD_DrawString((ILI9341_WIDTH - temp_w) / 2, box_y + 55,
                   temp_str, LCD_VALUE_COLOR, LCD_BG_COLOR, FONT_LARGE);

    /* Decorative degree symbol line */
    LCD_DrawHLine(box_x + 30, box_y + box_h - 25, box_w - 60, COLOR_DARK_GRAY);
}

static void DisplayError(const char *message)
{
    /* Clear the display area */
    LCD_FillRect(0, 42, ILI9341_WIDTH, ILI9341_HEIGHT - 62, LCD_BG_COLOR);

    /* Show error in large text */
    uint16_t msg_w = strlen(message) * LCD_CharWidth(FONT_MEDIUM);
    LCD_DrawString((ILI9341_WIDTH - msg_w) / 2, 120,
                   message, LCD_ERROR_COLOR, LCD_BG_COLOR, FONT_MEDIUM);

    /* Turn on red LED for hardware error indication */
    HAL_GPIO_WritePin(LED_PORT, LED_RED_PIN, GPIO_PIN_SET);
}

/* --------------------------------------------------------------------------
 * Error Handler
 * -------------------------------------------------------------------------- */
static void Error_Handler(void)
{
    /* Disable interrupts */
    __disable_irq();

    /* Blink red LED rapidly to indicate error */
    while (1) {
        HAL_GPIO_TogglePin(LED_PORT, LED_RED_PIN);
        for (volatile uint32_t i = 0; i < 2000000; i++) { /* ~100ms delay */ }
    }
}

/* --------------------------------------------------------------------------
 * Main Application
 * -------------------------------------------------------------------------- */
int main(void)
{
    float temperature = 0.0f;
    bool  lm75_ok     = false;
    uint32_t last_read_tick = 0;

    /* ---- HAL Initialization ---- */
    HAL_Init();

    /* ---- System Clock Configuration ---- */
    SystemClock_Config();

    /* ---- Initialize all peripherals ---- */
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_SPI2_Init();

    /* ---- Turn on green LED briefly to show power-on ---- */
    HAL_GPIO_WritePin(LED_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(LED_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);

    /* ---- Initialize the ILI9341 LCD ---- */
    if (!LCD_Init(&hspi2)) {
        DisplayError("LCD Init Failed!");
        Error_Handler();
    }

    /* Clear the screen and draw persistent UI elements */
    LCD_FillScreen(LCD_BG_COLOR);
    DrawHeader();
    UpdateStatusBar("Initializing LM75 sensor...");

    /* ---- Initialize the LM75 temperature sensor ---- */
    HAL_Delay(50);  /* Brief delay to allow I2C bus to stabilize */

    if (LM75_Init(&hi2c1)) {
        lm75_ok = true;

        /* Do a test read to verify the sensor is working */
        if (LM75_ReadTemperature(&temperature)) {
            UpdateStatusBar("LM75 OK - Connected at 0x48");
            DisplayTemperature(temperature);
        } else {
            UpdateStatusBar("LM75 detected but read failed");
            DisplayError("Read Error");
        }
    } else {
        lm75_ok = false;
        UpdateStatusBar("LM75 NOT FOUND at 0x48");
        DisplayError("LM75 Not Found!");
    }

    /* ---- Main application loop ---- */
    while (1) {
        uint32_t current_tick = HAL_GetTick();

        /* Read temperature at regular intervals */
        if ((current_tick - last_read_tick) >= TEMP_READ_INTERVAL_MS) {
            last_read_tick = current_tick;

            /* Toggle orange LED to show the application is alive */
            HAL_GPIO_TogglePin(LED_PORT, LED_ORANGE_PIN);

            if (lm75_ok) {
                if (LM75_ReadTemperature(&temperature)) {
                    /* Display the temperature reading */
                    DisplayTemperature(temperature);
                    UpdateStatusBar("Reading OK");
                } else {
                    /* I2C read failed - display error */
                    DisplayError("Read Error");
                    UpdateStatusBar("I2C read failed - check wiring");

                    /* Try to re-initialize the LM75 */
                    LM75_Init(&hi2c1);
                }
            } else {
                /* Retry LM75 initialization */
                if (LM75_Init(&hi2c1)) {
                    lm75_ok = true;
                    UpdateStatusBar("LM75 re-connected at 0x48");

                    if (LM75_ReadTemperature(&temperature)) {
                        DisplayTemperature(temperature);
                    }
                }
            }
        }
    }
}

/* --------------------------------------------------------------------------
 * HAL Callback: SysTick Handler
 * -------------------------------------------------------------------------- */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
