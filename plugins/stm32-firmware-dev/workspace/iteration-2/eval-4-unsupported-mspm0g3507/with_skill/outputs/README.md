# MSPM0G3507 Temperature-Controlled Fan (Bare-Metal)

Simple bare-metal firmware that reads the internal temperature sensor of
MSPM0G3507 and controls a fan via GPIO with hysteresis.

## Phase -1: Chip Compatibility (MANUAL ONLY Quadrant)

```
           │ Docs MCP ✅          │ Docs MCP ❌
───────────┼──────────────────────┼─────────────────────────
Sim MCP ✅ │ FULL MCP (e.g. F103) │ Sim-only (e.g. F407)
───────────┼──────────────────────┼─────────────────────────
Sim MCP ❌ │ Docs-only            │ MANUAL ONLY  ◀ MSPM0G3507
```

- **Docs MCP**: Only STM32F103C8 indexed. MSPM0G3507 is a TI chip.
- **Sim MCP**: No QEMU machine for TI MSPM0 series.
- **Impact**: All register values from web research. No simulation possible.
  Code cannot be tested until real MSPM0G3507 hardware arrives.

## Function

| Condition | Fan State |
|-----------|-----------|
| Temperature > 30C | ON  (PA0 high) |
| Temperature < 25C | OFF (PA0 low) |
| 25C <= T <= 30C  | Maintain current state (5C hysteresis) |

## Pin Assignment

| MSPM0G3507 Pin | Function | Direction | Connection |
|----------------|----------|-----------|------------|
| PA0 | Fan control | Output | Fan MOSFET/BJT gate (via 1k resistor) |

The fan MUST be driven through a transistor/MOSFET — do NOT connect
a fan directly to the GPIO pin (max 8mA source/sink).

## Temperature Sensor

- Internal MSPM0G3507 temperature sensor on ADC0 channel 11
- Factory calibration at 30C (TEMP_SENSE0.DATA @ 0x41C4003C)
- 12-bit ADC resolution with VDDA=3.3V reference
- Coefficient: -1.8 mV/C (voltage decreases as temperature rises)

## Build

```bash
# Requires arm-none-eabi-gcc
make all

# Outputs:
#   build/temp_fan.elf   — ELF with debug symbols
#   build/temp_fan.hex   — Intel HEX for flashing
#   build/temp_fan.bin   — Raw binary
```

## Flash (LP-MSPM0G3507 LaunchPad)

```bash
make flash
```

Assumes CMSIS-DAP debug probe (XDS110 on LaunchPad) and OpenOCD.

## Register Sources (NOT MCP-verified)

| Source | Document ID | Purpose |
|--------|-------------|---------|
| TI TRM | SLAU846B | Full register map, ADC/GPIO bitfields |
| TI Datasheet | SLASEX6 | Pinout, peripheral base addresses, temp sensor params |
| TI E2E Forum | — | Confirmed VDDA=3.3V calibration ref (TRM bug) |

## Known Limitations

1. **Not MCP-verified**: Register values from web research, not from the
   knowledge graph. There may be errors in bit positions or register offsets.
2. **No simulation**: QEMU does not support MSPM0. Cannot verify register
   writes without hardware.
3. **Minimal startup**: Only copies .data and zeroes .bss. No clock tree
   reconfiguration (uses default 32MHz SYSOSC).
4. **No interrupts**: Uses polling-only for both ADC and delay.
5. **SysTick as delay timer**: Blocks CPU during delay loops.
