---
name: stm32-firmware-dev
description: >
  Complete STM32 firmware development workflow using MCP-assisted documentation lookup,
  QEMU simulation verification, and serial hardware debugging. Use this skill whenever the user
  wants to develop STM32 firmware, write embedded drivers, integrate sensors (I2C, SPI, UART),
  build projects for Cortex-M MCUs, create STM32 examples, implement sensor fusion
  algorithms on MCUs, or debug STM32 register configurations. This skill MUST be used when the
  user mentions STM32 chip models (STM32F1xx, STM32F4xx, etc.), embedded peripherals (GPIO, I2C,
  SPI, USART, TIM, ADC, DMA), MCU sensor integration (MPU6050, HMC5883L, OLED SSD1306, etc.),
  or asks to create firmware/embedded projects. Trigger even if the user doesn't explicitly say
  "skill" or "firmware" — any mention of STM32 or MCU peripheral configuration should activate
  this skill.
---

# STM32 Firmware Development Skill

Integrates three MCP services into a single development pipeline:

1. **embedded-docs** — query STM32 register maps, pin assignments, clock trees from the knowledge graph
2. **simulation** — run firmware in QEMU and verify register initialization via GDB
3. **serial-debug** — flash to hardware and capture serial output from sensors

## Core Philosophy: Hardware Truth × HAL Reliability

The knowledge graph knows **which** clocks to enable, **which** pins to configure, and **which** alternate functions to set — hardware truth that prevents silent failures like "I2C hangs because APB1 clock wasn't enabled."

HAL provides battle-tested register access patterns that handle edge cases correctly (volatile ordering, read-modify-write sequences, errata workarounds).

**Together**: you get HAL code that actually works because every `__HAL_RCC_*_CLK_ENABLE()` call is backed by graph-verified bus topology, and every `HAL_GPIO_Init()` struct is populated with graph-verified pin/alternate-function values.

**Default is HAL. Bare-metal only when the user explicitly asks for it** (e.g., "不用 HAL", "bare metal", "寄存器级编程", "no HAL").

## Workflow Overview

Always follow this sequence. The user may not know about all three MCP services — you are responsible for introducing each phase and explaining what it catches.

```
Phase -1: Chip Compatibility Check   (Docs MCP + Simulation MCP — BOTH required)
Phase  0: Documentation Research     (embedded-docs MCP)
Phase  1: Code Generation            (HAL by default — your expertise + graph knowledge)
Phase  2: Simulation Verification    (simulation MCP, if available)
Phase  3: Hardware Testing           (serial-debug MCP, optional — only if HW ready)
```

---

## Phase -1: Chip Compatibility Check

Before writing ANY code, check TWO things in parallel. This is the most critical phase — skipping it leads to wrong register values, broken simulation, or silent failures.

### Step 1: Dual MCP Query

Call BOTH queries in the same turn:

1. **`mcp__embedded-docs__embedded_docs_list_chips`** — Is the chip in the knowledge base? Can we get graph-verified clock/pin/register data?
2. **`mcp__simulation__list_supported_chips`** — Is there a QEMU machine for this chip? Can we simulate it?

### Step 2: Build Compatibility Matrix

Report this table to the user BEFORE generating any code:

```
           │ Docs MCP ✅          │ Docs MCP ❌
───────────┼──────────────────────┼─────────────────────
Sim MCP ✅ │ FULL MCP (e.g. F103) │ Sim-only (e.g. F407)
           │ Graph-verified clock │ General knowledge +
           │ + pin values. Both   │ QEMU verification.
           │ sim + docs ok.       │ No doc verification.
───────────┼──────────────────────┼─────────────────────
Sim MCP ❌ │ Docs-only            │ MANUAL ONLY
           │ Graph-verified data  │ No doc, no sim.
           │ but no sim verify.   │ Warn user clearly.
```

### Step 3: Adapt Strategy Per Quadrant

**FULL MCP (Docs ✅ + Sim ✅)** — Best case. Use embedded-docs for all clock enable/pin mapping data. This feeds directly into HAL config structs. Use simulation for GDB verification. Add `#ifdef SIMULATION` for any QEMU/hardware clock differences.

**Sim-only (Docs ❌ + Sim ✅)** — Can verify register writes via GDB, but clock/pin data comes from general knowledge (datasheet training data). Add a code comment banner noting: `// Clock/pin configuration from general knowledge — NOT graph-verified`. Simulation still catches GPIO config errors, clock hangs, and bus faults.

**Docs-only (Docs ✅ + Sim ❌)** — Graph-verified data but no simulation. Trust the clock/pin mappings but can't verify them in QEMU. Code can only be tested on real hardware.

**MANUAL ONLY (Docs ❌ + Sim ❌)** — Warn the user that:
- No graph-verified clock/pin data available
- No QEMU simulation possible
- All values from general knowledge
- Code cannot be tested until hardware arrives
- Suggest the user consider adding this chip to the knowledge base pipeline

### Step 4: Simulation Chip vs Target Chip Comparison

Even when both MCPs support the chip, the QEMU machine may emulate a DIFFERENT chip. Always compare:

| Aspect | Target Chip | Simulation Chip | Impact |
|--------|:----------:|:-------------:|--------|
| CPU | ... | ... | same/different ISA |
| Max frequency | ... | ... | PLL config may differ — use `#ifdef SIMULATION` |
| Flash/RAM | ... | ... | Linker must match TARGET, not emulated chip |
| Key peripherals | ... | ... | Which registers can be verified in sim |

### Step 5: Clock Adaptation Strategy

When target and simulation chips differ in max frequency:

1. Use a compile-time `#ifdef SIMULATION` flag for clock init
2. Simulation: use HSI (8MHz internal) directly, skip PLL
3. Hardware: use full PLL config (e.g., HSE 8MHz → PLL×9 → 72MHz)

---

## Phase 0: Documentation Research

Query the embedded-docs MCP for all clock/pin/register parameters needed by the project. These answers feed directly into HAL init structs.

**Query strategy** — make at least these 3 queries (in parallel):

1. **Pin mapping**: `"<chip> <peripheral> pin mapping: which GPIO pins are used, and what alternate function configuration is needed?"`
   - `intent_hint`: `pin_mapping`
   - Extract: pin names, alternate function numbers (AF0-AF15), which GPIO port — feeds `GPIO_InitTypeDef.Alternate`

2. **Clock tree / bus topology**: `"<chip> system clock: which bus (APB1/APB2/AHB) is <peripheral> on? What clock enable bit does it use?"`
   - `intent_hint`: `clock_tree`
   - Extract: bus domain, clock enable register + bit — feeds `__HAL_RCC_*_CLK_ENABLE()` macros and `HAL_RCC_Get*Freq()` for timing calculations

3. **Peripheral configuration**: `"<peripheral> control registers: what timing/prescaler values for <target_speed>?"`
   - `intent_hint`: `register_lookup`
   - Extract: prescaler/timing register formulas — feeds HAL init struct fields (`I2C_InitTypeDef.Timing`, `SPI_InitTypeDef.BaudRatePrescaler`)

**Additional queries** as needed per project:
- I2C timing: CCR/TRISE calculation for target bus frequency
- SPI baud rate: BR[2:0] prescaler values
- Timer/PWM: PSC/ARR calculation
- Interrupts: NVIC priority group and IRQ numbers (embedded-docs may not cover NVIC — use general knowledge)

**Save results**: Summarize findings in a comment block at the top of each driver file. This gives the user a reference for why specific values were chosen.

---

## Phase 1: Code Generation (HAL by Default)

Generate STM32 firmware using the **HAL library by default**. The knowledge graph from Phase 0 tells you **which** clocks and pins — HAL provides the **correct way** to configure them.

**Switch to bare-metal ONLY when the user explicitly requests it** (e.g., "不用 HAL", "bare metal", "寄存器级", "no HAL"). See the Bare-Metal Reference appendix at the end of this document.

### The Graph→HAL Bridge

This is the core pattern: every piece of Phase 0 data maps to a specific HAL API call:

| Phase 0 Knowledge | HAL Usage |
|-------------------|-----------|
| "I2C1 is on APB1, clock enable bit 21" | `__HAL_RCC_I2C1_CLK_ENABLE()` before `HAL_I2C_Init()` |
| "PB6 = I2C1_SCL, AF4 open-drain" | `GPIO_InitTypeDef` with `Pin=GPIO_PIN_6`, `Alternate=GPIO_AF4`, `Mode=GPIO_MODE_AF_OD` |
| "USART1 is on APB2, TX=PA9 AF7" | `__HAL_RCC_USART1_CLK_ENABLE()`, then `HAL_UART_Init()` with 115200 baud |
| "SPI1 is on APB2, SCK=PA5 MOSI=PA7" | `__HAL_RCC_SPI1_CLK_ENABLE()`, then `HAL_SPI_Init()` with prescaler from Phase 0 |
| "APB1 timer clock = 2× APB1 freq when prescaler ≠ 1" | Feed correct APB1 timer clock into `HAL_TIM_Base_Init()` prescaler calculation |

### Project Structure

Create files relative to the user's current working directory, or wherever they specify. Do NOT force projects into a hardcoded parent directory.

```
<project_name>/
├── README.md                     # Pin assignment + wiring + build/flash instructions
├── Makefile                      # arm-none-eabi-gcc build with HAL_DRIVER_PATH
├── <chip>_flash.ld              # Linker script (Flash/RAM sizes from chip datasheet)
├── startup_<chip>.s             # Startup file (vector table + Reset_Handler)
├── system_<chip>.c              # SystemInit() — calls HAL_Init() + SystemClock_Config()
├── stm32f1xx_hal_conf.h         # HAL module configuration (enable needed modules)
└── src/
    ├── main.c                   # Main entry + main loop
    ├── main.h                   # Clock frequency macros (HSE_VALUE, SYSCLK, etc.)
    ├── <bus>_bus.c/h            # Bus drivers (i2c_bus, spi_bus, uart_bus)
    └── <sensor>.c/h             # Sensor/device drivers
```

### HAL Coding Standards

**1. Clock enable before peripheral init** — Always call `__HAL_RCC_*_CLK_ENABLE()` for both the peripheral AND its GPIO banks before any `HAL_*_Init()`:

```c
// From Phase 0: I2C1 is on APB1, PB6+PB7 are on GPIOB (APB2)
__HAL_RCC_GPIOB_CLK_ENABLE();   // GPIOB is on APB2
__HAL_RCC_I2C1_CLK_ENABLE();    // I2C1 is on APB1

// Now safe to call:
HAL_GPIO_Init(&hi2c1_sda);      // PB7
HAL_GPIO_Init(&hi2c1_scl);      // PB6
HAL_I2C_Init(&hi2c1);
```

**2. GPIO configuration** — Phase 0 tells you the alternate function number and mode. Use it directly:

```c
// Phase 0 result: "PB6 = I2C1_SCL, alternate function AF4, open-drain, max 50MHz"
GPIO_InitTypeDef gpio = {0};
gpio.Pin = GPIO_PIN_6;
gpio.Mode = GPIO_MODE_AF_OD;        // open-drain for I2C
gpio.Pull = GPIO_NOPULL;            // external pull-ups required (graph gotcha #3)
gpio.Speed = GPIO_SPEED_FREQ_HIGH;  // 50MHz
gpio.Alternate = GPIO_AF4;          // from Phase 0 pin mapping query
HAL_GPIO_Init(GPIOB, &gpio);
```

**3. I2C initialization** — Use `HAL_I2C_Init()` with timing from Phase 0 clock tree data:

```c
I2C_HandleTypeDef hi2c1;
hi2c1.Instance = I2C1;
hi2c1.Init.ClockSpeed = 100000;     // 100kHz standard mode
hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
// I2C timing from Phase 0: CCR = PCLK1_MHz * 10us / 2
// For 36MHz APB1: CCR = 180, TRISE = 37
hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
HAL_I2C_Init(&hi2c1);
```

**4. SPI initialization** — Phase 0 tells you the prescaler:

```c
SPI_HandleTypeDef hspi1;
hspi1.Instance = SPI1;
hspi1.Init.Mode = SPI_MODE_MASTER;
hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; // from Phase 0
hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
HAL_SPI_Init(&hspi1);
```

**5. System clock configuration** — Phase 0 clock tree query gives you the complete RCC setup:

```c
void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;         // HSE 8MHz × 9 = 72MHz, from Phase 0
    HAL_RCC_OscConfig(&osc);

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;    // APB1 max 36MHz, from Phase 0
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}
```

### Makefile Template (HAL)

```makefile
# User must set HAL_DRIVER_PATH to STM32CubeF1 location
HAL_DRIVER_PATH ?= ../Drivers/STM32F1xx_HAL_Driver

CFLAGS += -DUSE_HAL_DRIVER -DSTM32F103xB
CFLAGS += -I$(HAL_DRIVER_PATH)/Inc

HAL_SRCS  = $(HAL_DRIVER_PATH)/Src/stm32f1xx_hal.c
HAL_SRCS += $(HAL_DRIVER_PATH)/Src/stm32f1xx_hal_rcc.c
HAL_SRCS += $(HAL_DRIVER_PATH)/Src/stm32f1xx_hal_gpio.c
HAL_SRCS += $(HAL_DRIVER_PATH)/Src/stm32f1xx_hal_i2c.c
HAL_SRCS += $(HAL_DRIVER_PATH)/Src/stm32f1xx_hal_spi.c
HAL_SRCS += $(HAL_DRIVER_PATH)/Src/stm32f1xx_hal_uart.c
HAL_SRCS += $(HAL_DRIVER_PATH)/Src/stm32f1xx_hal_cortex.c

# Only include HAL sources for peripherals actually used
OBJS += $(HAL_SRCS:.c=.o)
```

If `HAL_DRIVER_PATH` is not set, prompt the user to set it or download STM32CubeF1 from ST.com.

### README Template

Every project README must include:
1. **Pin assignment table**: STM32 pin → peripheral pin → function → alternate function → HAL config
2. **Wiring diagram** (text-based): how to connect sensors to the board
3. **Build instructions**: `make HAL_DRIVER_PATH=<path> all`
4. **Flash instructions**: DAPlink / ST-Link / serial bootloader

---

## Phase 2: Simulation Verification

After each logical code chunk compiles, verify register initialization in QEMU. HAL code still writes to the same hardware registers — GDB verification works identically.

**Step 1 — Build**: Run `make all` to produce the ELF file.

**Step 2 — Start simulation**: Call `mcp__simulation__start_sim` with the ELF path and chip model (from Phase -1 compatibility mapping).

**Step 3 — Verify with GDB**: Use `mcp__simulation__send_gdb_cmd` to check critical registers:

```
# Verify clock configuration
x/4x 0x40021000    # RCC_CR — check HSEON/PLLON/HSERDY/PLLRDY bits
x/4x 0x40021004    # RCC_CFGR — check SYSCLK source, prescalers
x/4x 0x40021018    # RCC_APB2ENR — GPIO and APB2 peripheral clock enables
x/4x 0x4002101C    # RCC_APB1ENR — APB1 peripheral clock enables

# Verify GPIO configuration
x/4x 0x40010800    # GPIOA_CRL — check PA0-PA7 mode/config
x/4x 0x40010C00    # GPIOB_CRL — check PB0-PB7 mode/config

# Verify I2C/SPI configuration
x/4x 0x40005400    # I2C1_CR1 — check PE bit
x/4x 0x40013000    # SPI1_CR1 — check SPE/MSTR/BR bits

# Check program flow
info registers     # verify PC is in expected range
```

**What simulation can catch**:
- Clock stuck (PLLRDY never asserted)
- GPIO config errors (wrong CNF/MODE values — HAL struct misconfiguration)
- Peripheral not clocked (register writes have no effect — missed `__HAL_RCC_*_CLK_ENABLE()`)
- Bus fault / hard fault (PC jumps to fault handler)

**What simulation CANNOT catch** (needs real hardware):
- I2C/SPI communication with actual sensors
- Interrupt timing
- Analog behavior (ADC, temperature sensor)

**Step 4 — Clean up**: Call `mcp__simulation__stop_sim` when done.

---

## Phase 3: Hardware Testing

Only when the user confirms hardware is ready.

**Flashing**: For DAPlink, the binary is copied to the virtual USB drive. For ST-Link, use `st-flash`. The Makefile should have a `flash` target.

**Serial verification**: Use `mcp__serial-debug__serial_list_ports` to find the serial port, then `serial_open` + `serial_read` to capture printf/sensor output.

**Physical verification checklist** (add to project README):
- [ ] I2C devices detected at expected addresses (scan bus)
- [ ] Sensor WHO_AM_I / ID registers match datasheet
- [ ] Data values change with physical movement
- [ ] Display shows correct output

---

## Chip Support Matrix

The current embedded-docs MCP covers STM32F103 (RM0008). When the user requests an unsupported chip family:

1. Query `embedded_docs_list_chips` to confirm what's available
2. Report that graph-verified queries only work for F103
3. For unsupported families, fall back to general knowledge and the chip's reference manual
4. Note this limitation clearly in generated code comments

| Chip Family | embedded-docs | simulation (QEMU) | Notes |
|-------------|:---:|:---:|-------|
| STM32F103xx | ✅ full | ✅ stm32vldiscovery (F100) | PLL mismatch in sim |
| STM32F0xx | ❌ | ❌ | General knowledge only |
| STM32F4xx | ❌ | ✅ netduinoplus2 (F407) | Register layout differs from F1 |
| STM32L4xx | ❌ | ❌ | General knowledge only |

When the user asks to add support for a new chip family, remind them they need to:
1. Add the SVD file to the knowledge graph pipeline
2. Parse the reference manual into document chunks
3. Find or configure a QEMU machine model

---

## Key Constraints and Gotchas

These come from the MCP knowledge base — always apply them regardless of HAL or bare-metal:

1. **GPIO clock must be enabled first**: `__HAL_RCC_GPIOx_CLK_ENABLE()` before GPIO register writes
2. **FLASH_ACR latency before PLL**: Set `FLASH_LATENCY_*` according to target frequency before enabling PLL
3. **I2C external pull-ups required**: STM32 internal pull-ups (~40kΩ) are too weak for I2C — use `GPIO_NOPULL` and external 4.7kΩ resistors
4. **APB1 max 36MHz**: PPRE1 must divide by ≥2 when SYSCLK > 36MHz
5. **APB1 timer clock doubling**: When APB1 prescaler ≠ 1, timer clocks run at 2× APB1 frequency
6. **Cortex-M3 has no hardware FPU**: Link with `-lm` for software floating-point emulation
7. **I2C CCR formula**: CR2.FREQ is APB1 MHz (not a divider), CCR = FREQ × t_SCL / 2

---

## Appendix: Bare-Metal Path (Explicit Request Only)

Only use this when the user explicitly says "不用 HAL", "bare metal", "寄存器级编程", or "no HAL".

### Bare-Metal Project Structure

```
<project_name>/
├── README.md
├── Makefile
├── <chip>.ld                    # Linker script
├── startup_<chip>.s             # Startup file (vector table + Reset_Handler)
└── src/
    ├── main.c                   # Main entry + main loop
    ├── main.h                   # Clock frequency macros
    ├── rcc.c/h                  # System clock configuration
    ├── systick.c/h              # SysTick timer (1ms tick)
    ├── <bus>_bus.c/h            # Bus drivers
    └── <sensor>.c/h             # Sensor/device drivers
```

### Bare-Metal Register Access

Use `volatile uint32_t*` pointers to memory-mapped addresses. Define peripheral base addresses as macros:

```c
#define RCC_BASE     0x40021000UL
#define RCC_CR       (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_CFGR     (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_APB2ENR  (*(volatile uint32_t*)(RCC_BASE + 0x18))
```

### GPIO Configuration (Bare-Metal)

Each pin is 4 bits in CRL (pins 0-7) or CRH (pins 8-15):
- CNF[1:0] + MODE[1:0]
- I2C open-drain: CNF=11, MODE=11 → 0xF
- SPI push-pull: CNF=10, MODE=11 → 0xB
- GPIO output: CNF=00, MODE=11 → 0x3
- Analog/input: CNF=00, MODE=00 → 0x0

### Bare-Metal Makefile

Use arm-none-eabi-gcc with `-mcpu=cortex-m3 -mthumb -O2 -g`. No `-DUSE_HAL_DRIVER`, no HAL source files.
