# STM32F407VGT6 Discovery — LM75 I2C Temperature Sensor + LCD

## Chip Compatibility

|              | Docs MCP        | Sim MCP         |
|--------------|:---------------:|:---------------:|
| **Result**   | UNSUPPORTED     | SUPPORTED       |
| **Details**  | Only STM32F103C8 is indexed in knowledge base | STM32F407VG → netduinoplus2 QEMU machine |

**This project falls in the "Sim-only" quadrant.** Register values come from general knowledge (STM32F4 reference manuals). No MCP-verified register values are available. QEMU simulation can still verify clock init, GPIO config, and I2C register writes.

## Limitation Notes

1. **Docs MCP does not cover STM32F4xx** — All register addresses and bitfield values in this firmware are based on general knowledge from the STM32F4 reference manual (RM0090). They have NOT been verified against the MCP knowledge graph.

2. **QEMU simulation available** — The `netduinoplus2` machine emulates an F4-like Cortex-M4. It can verify clock initialization, GPIO configuration, and I2C peripheral register writes. It cannot test actual I2C communication with an LM75 sensor.

3. **F4 vs F1 register differences** — Key differences from F1 to watch for:
   - GPIO clock enable: AHB1ENR (F4) vs APB2ENR (F1)
   - GPIO config: MODER/OTYPER/OSPEEDR/PUPDR/AFR (F4) vs CRL/CRH (F1)
   - RCC base: 0x40023800 (F4) vs 0x40021000 (F1)
   - PLL config: PLLCFGR with PLLM/PLLN/PLLP (F4) vs PLLMUL in CFGR (F1)

4. **LCD caveat** — The STM32F4DISCOVERY (STM32F407G-DISC1) does NOT include a built-in LCD. The ILI9341 driver is included as a reference. If your board has no LCD, the SPI output is harmless. Consider using UART for serial output instead.

## Pin Assignment

### I2C1 (LM75 Temperature Sensor)

| STM32 Pin | Function | LM75 Pin | GPIO Config |
|-----------|----------|----------|-------------|
| PB6       | I2C1_SCL | SCL      | AF4, Open-Drain, No Pull-up |
| PB7       | I2C1_SDA | SDA      | AF4, Open-Drain, No Pull-up |
| 3.3V      | Power    | VCC      | —           |
| GND       | Ground   | GND      | —           |
| GND       | Address  | A0, A1, A2 | LM75 addr = 0x48 |

### SPI1 (ILI9341 LCD — reference only)

| STM32 Pin | Function  | LCD Pin | GPIO Config |
|-----------|-----------|---------|-------------|
| PA5       | SPI1_SCK  | SCK     | AF5         |
| PA7       | SPI1_MOSI | MOSI    | AF5         |
| PA6       | SPI1_MISO | MISO    | AF5 (unused)|
| PC4       | GPIO Out  | CS      | Push-Pull   |
| PC5       | GPIO Out  | DC      | Push-Pull   |
| PC6       | GPIO Out  | RST     | Push-Pull   |

### I2C Wiring

```
STM32F407 Discovery          LM75
+-------------------+       +------+
|             PB6   |-------| SCL  |
|             PB7   |-------| SDA  |
|             3.3V  |-------| VCC  |
|             GND   |-------| GND  |
|             GND   |-------| A0   |
|             GND   |-------| A1   |
|             GND   |-------| A2   |
+-------------------+       +------+
```

External pull-up resistors (4.7kΩ) required on SCL and SDA to 3.3V.

## Clock Configuration

```
HSE (8MHz crystal) → /PLLM(8) → ×PLLN(336) → /PLLP(2) → 168MHz SYSCLK
AHB  = SYSCLK / 1  = 168MHz (HCLK)
APB1 = SYSCLK / 4  =  42MHz (PCLK1)
APB2 = SYSCLK / 2  =  84MHz (PCLK2)
Flash wait states: 5 (LATENCY=5) for 168MHz @ 3.3V
```

## I2C1 Configuration

- **Speed**: Standard mode (100kHz)
- **APB1 clock**: 42MHz
- **CR2.FREQ**: 42 (MHz)
- **CCR**: 210 (= 42MHz / (2 × 100kHz))
- **TRISE**: 43 (= 1000ns / 23.8ns + 1)

## Build Instructions

```bash
# Prerequisites: arm-none-eabi-gcc toolchain
make all      # Build ELF + HEX + BIN
make flash    # Flash via ST-Link (st-flash)
make clean    # Remove build artifacts
```

## Flash Instructions

### ST-Link/V2
```bash
make flash
# Or manually:
st-flash --format ihex write lm75_f407.hex
```

### DAPlink (copy to virtual USB drive)
```bash
cp lm75_f407.bin /media/<user>/DAPLINK/
```

## LM75 Temperature Data Format

- 2 bytes (MSB first) from register 0x00
- 9-bit two's complement value
- Resolution: 0.5°C per bit
- Example: 0x19 0x00 → 25.0°C
- Example: 0xE7 0x00 → -25.0°C

## Simulation Verification

```bash
# Build for simulation
CFLAGS="-DSIMULATION" make all

# Start QEMU simulation (using MCP tools)
# mcp__simulation__start_sim with chip_model="STM32F407VG"

# Verify registers via GDB:
# x/4x 0x40023800    # RCC_CR (base different from F1!)
# x/4x 0x40023804    # RCC_PLLCFGR
# x/4x 0x40023808    # RCC_CFGR
# x/4x 0x40020400    # GPIOB_MODER (check PB6/PB7 are AF)
# x/4x 0x40005400    # I2C1_CR1 (check PE=1)
```

## Project Structure

```
outputs/
├── README.md
├── Makefile
├── STM32F407VG.ld          # Linker script (1024K Flash, 128K RAM)
├── startup_stm32f407.s     # Vector table + Reset_Handler (Cortex-M4)
└── src/
    ├── main.c              # Main loop: read LM75, display on LCD
    ├── main.h              # Clock macros, peripheral base addresses
    ├── rcc.c/h             # 168MHz PLL clock configuration
    ├── systick.c/h         # SysTick 1ms timer
    ├── i2c_bus.c/h         # I2C1 master driver (PB6/PB7)
    ├── lm75.c/h            # LM75 temperature sensor driver
    ├── lcd_ili9341.c/h     # ILI9341 TFT LCD driver (SPI mode)
    └── font.h              # 8x16 bitmap font for LCD text
```

## Register Address Verification (F4 vs F1)

| Peripheral | F103 (Docs MCP) | F407 (This Project) |
|-----------|:---------------:|:-------------------:|
| RCC       | 0x40021000      | **0x40023800**      |
| GPIOA     | 0x40010800      | **0x40020000**      |
| GPIOB     | 0x40010C00      | **0x40020400**      |
| GPIOC     | 0x40011000      | **0x40020800**      |
| I2C1      | 0x40005400      | 0x40005400 (same)   |
| SPI1      | 0x40013000      | 0x40013000 (same)   |
| FLASH     | 0x40022000      | **0x40023C00**      |
| GPIO clk  | APB2ENR (F1)    | **AHB1ENR (F4)**    |
