---
name: stm32-firmware-dev
description: >
  Complete STM32 bare-metal firmware development workflow using MCP-assisted documentation lookup,
  QEMU simulation verification, and serial hardware debugging. Use this skill whenever the user
  wants to develop STM32 firmware, write embedded drivers, integrate sensors (I2C, SPI, UART),
  build bare-metal projects for Cortex-M MCUs, create STM32 examples, implement sensor fusion
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

## Workflow Overview

Always follow this sequence. The user may not know about all three MCP services — you are responsible for introducing each phase and explaining what it catches.

```
Phase -1: Chip Compatibility Check   (Docs MCP + Simulation MCP — BOTH required)
Phase  0: Documentation Research     (embedded-docs MCP)
Phase  1: Code Generation            (your expertise)
Phase  2: Simulation Verification    (simulation MCP, if available)
Phase  3: Hardware Testing           (serial-debug MCP, optional — only if HW ready)
```

---

## Phase -1: Chip Compatibility Check

Before writing ANY code, check TWO things in parallel. This is the most critical phase — skipping it leads to wrong register values, broken simulation, or silent failures.

### Step 1: Dual MCP Query

Call BOTH queries in the same turn:

1. **`mcp__embedded-docs__embedded_docs_list_chips`** — Is the chip in the knowledge base? Can we get MCP-verified register values?
2. **`mcp__simulation__list_supported_chips`** — Is there a QEMU machine for this chip? Can we simulate it?

### Step 2: Build Compatibility Matrix

Report this table to the user BEFORE generating any code:

```
           │ Docs MCP ✅          │ Docs MCP ❌
───────────┼──────────────────────┼─────────────────────
Sim MCP ✅ │ FULL MCP (e.g. F103) │ Sim-only (e.g. F407)
           │ Register values from │ General knowledge +
           │ knowledge graph.     │ QEMU verification.
           │ Both sim + docs ok.  │ No doc verification.
───────────┼──────────────────────┼─────────────────────
Sim MCP ❌ │ Docs-only            │ MANUAL ONLY
           │ MCP-verified regs    │ No doc, no sim.
           │ but no sim verify.   │ Warn user clearly.
```

### Step 3: Adapt Strategy Per Quadrant

**FULL MCP (Docs ✅ + Sim ✅)** — Best case. Use embedded-docs for all register values. Use simulation for GDB verification. Add `#ifdef SIMULATION` for any QEMU/hardware clock differences discovered in the comparison below.

**Sim-only (Docs ❌ + Sim ✅)** — Can verify register writes via GDB, but register values come from general knowledge (datasheet training data). Add a code comment banner noting this: `// Register values from general knowledge — NOT MCP-verified`. Simulation still catches GPIO config errors, clock hangs, and bus faults.

**Docs-only (Docs ✅ + Sim ❌)** — MCP-verified register values but no simulation. Trust the register numbers but can't verify them in QEMU. Code can only be tested on real hardware.

**MANUAL ONLY (Docs ❌ + Sim ❌)** — Warn the user that:
- No MCP-verified register values available
- No QEMU simulation possible
- All register values from general knowledge
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

Query the embedded-docs MCP for all register-level parameters needed by the project.

**Query strategy** — make at least these 3 queries (in parallel):

1. **Pin mapping**: `"<chip> <peripheral> pin mapping: which GPIO pins are used, and what alternate function configuration is needed?"`
   - `intent_hint`: `pin_mapping`
   - Extract: pin names, CNF/MODE values, remap options

2. **Register configuration**: `"<peripheral> control register bitfield layout: what bits control enable, mode, speed, etc.?"`
   - `intent_hint`: `register_lookup`
   - Extract: register offsets, bit positions, reset values

3. **Clock tree**: `"<chip> system clock configuration: how to configure HSE/PLL for <target_freq>MHz? What are APB1/APB2 prescaler values?"`
   - `intent_hint`: `clock_tree`
   - Extract: RCC register values, FLASH_ACR latency, APB prescalers

**Additional queries** as needed per project:
- I2C timing: CCR/TRISE calculation for target bus frequency
- SPI baud rate: BR[2:0] prescaler values
- Timer/PWM: PSC/ARR calculation
- Interrupts: NVIC priority group and IRQ numbers (embedded-docs may not cover NVIC — use general knowledge)

**Save results**: Summarize findings in a comment block at the top of each driver file, or in the project README. This gives the user a reference for why specific register values were chosen.

---

## Phase 1: Code Generation

Generate bare-metal C firmware. No HAL dependency — all register-level access.

### Project Structure

Always create this directory layout under `examples/<project_name>/`:

```
examples/<project_name>/
├── README.md                    # Pin assignment + wiring + build/flash instructions
├── Makefile                     # arm-none-eabi-gcc build
├── <chip>.ld                    # Linker script (Flash/RAM sizes from chip datasheet)
├── startup_<chip>.s             # Startup file (vector table + Reset_Handler)
└── src/
    ├── main.c                   # Main entry + main loop
    ├── main.h                   # Clock frequency macros (HSE_VALUE, SYSCLK, etc.)
    ├── rcc.c/h                  # System clock configuration
    ├── systick.c/h              # SysTick timer (1ms tick, delay_ms, millis)
    ├── <bus>_bus.c/h            # Bus drivers (i2c_bus, spi_bus, uart_bus)
    ├── <sensor>.c/h             # Sensor/device drivers
    └── ...                      # Algorithm files (sensor_fusion, calibration, etc.)
```

### Coding Standards for Bare-Metal STM32

**Register access** — use `volatile uint32_t*` pointers to memory-mapped addresses. Define peripheral base addresses as macros:

```c
#define RCC_BASE     0x40021000UL
#define RCC_CR       (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_CFGR     (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_APB2ENR  (*(volatile uint32_t*)(RCC_BASE + 0x18))
```

**GPIO configuration** — each pin is 4 bits in CRL (pins 0-7) or CRH (pins 8-15):
- CNF[1:0] + MODE[1:0]
- I2C open-drain: CNF=11, MODE=11 → 0xF
- SPI push-pull: CNF=10, MODE=11 → 0xB
- GPIO output: CNF=00, MODE=11 → 0x3
- Analog/input: CNF=00, MODE=00 → 0x0

**Clock enable before peripheral config**: Always set RCC_APB2ENR/APB1ENR bits for GPIO banks and peripherals before writing to their registers.

**I2C timing**: The formula for CCR (from MCP queries):
- Standard mode (100kHz): CCR = APB1_MHz × 10μs / 2
- TRISE = APB1_MHz + 1 (standard mode)

### Makefile Template

Always include these targets: `all` (elf+hex+bin), `flash`, `clean`. Use arm-none-eabi-gcc with `-mcpu=cortex-m3 -mthumb -O2 -g`.

### README Template

Every project README must include:
1. **Pin assignment table**: STM32 pin → peripheral pin → function → GPIO config
2. **Wiring diagram** (text-based): how to connect sensors to the board
3. **Build instructions**: `make all`
4. **Flash instructions**: DAPlink / ST-Link / serial bootloader

---

## Phase 2: Simulation Verification

After each logical code chunk compiles, verify register initialization in QEMU.

**Step 1 — Build**: Run `make all` to produce the ELF file.

**Step 2 — Start simulation**: Call `mcp__simulation__start_sim` with the ELF path and chip model (from Phase -1 compatibility mapping).

**Step 3 — Verify with GDB**: Use `mcp__simulation__send_gdb_cmd` to check critical registers:

```
# Verify clock configuration
x/4x 0x40021000    # RCC_CR — check HSEON/PLLON/HSERDY/PLLRDY bits
x/4x 0x40021004    # RCC_CFGR — check SYSCLK source, prescalers

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
- GPIO config errors (wrong CNF/MODE values)
- Peripheral not clocked (register writes have no effect)
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
2. Report that register-level queries only work for F103
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

These come from the MCP knowledge base — always apply them:

1. **GPIO clock must be enabled first**: `RCC_APB2ENR` IOPAEN/IOPBEN bits before GPIO register writes
2. **FLASH_ACR latency before PLL**: Set LATENCY bits according to target frequency before enabling PLL
3. **I2C external pull-ups required**: STM32 internal pull-ups (~40kΩ) are too weak for I2C
4. **APB1 max 36MHz**: PPRE1 must divide by ≥2 when SYSCLK > 36MHz
5. **APB1 timer clock doubling**: When APB1 prescaler ≠ 1, timer clocks run at 2× APB1 frequency
6. **Cortex-M3 has no hardware FPU**: Link with `-lm` for software floating-point emulation
7. **I2C CCR formula**: CR2.FREQ is APB1 MHz (not a divider), CCR = FREQ × t_SCL / 2
