# Chip Compatibility Reference

## Currently Supported in MCP

### embedded-docs MCP
- STM32F103xx series (RM0008 Rev 21): Full register-level documentation for RCC, GPIO, I2C1/2, SPI1, USART1/2, TIM2, ADC1, DMA1, FLASH, NVIC

### simulation MCP (QEMU)
| QEMU Machine | Emulated Chip | CPU | Flash | RAM | Max MHz |
|-------------|--------------|-----|-------|-----|---------|
| stm32vldiscovery | STM32F100RBT6 | Cortex-M3 | 128KB | 8KB | 24 |
| netduinoplus2 | STM32F407VG | Cortex-M4 | 1024KB | 128KB | 168 |
| olimex-stm32-h405 | STM32F405RG | Cortex-M4 | 1024KB | 128KB | 168 |

### Target-to-QEMU Mapping
| Target Chip | Closest QEMU Machine | Known Differences |
|------------|---------------------|-------------------|
| STM32F103C8 | stm32vldiscovery | F100 vs F103: max 24MHz vs 72MHz, Value Line peripherals subset |
| STM32F103RBT6 | stm32vldiscovery | Same as above |
| STM32F407VG | netduinoplus2 | Exact match |
| STM32F405RG | olimex-stm32-h405 | Exact match |

## Simulation Compatibility Notes

### STM32F103 on stm32vldiscovery
- **Clock**: The F100 emulation maxes at 24MHz. PLL ×9 to reach 72MHz will NOT work — PLLRDY never asserts.
- **Fix**: Use `#ifdef SIMULATION` to run from HSI 8MHz instead of HSE+PLL.
- **Peripherals**: Basic GPIO, I2C, SPI, USART are register-compatible. Advanced timers and ADC may differ.
- **Flash/RAM**: F100 has less RAM (8KB vs 20KB). Linker script must match the real target, not the emulated one.

### Adding New Chip Families
When adding a new chip to the knowledge base:
1. Obtain the CMSIS-SVD file → `data/svd/<chip>.svd`
2. Parse with `svd_parser.py` → graph nodes + edges
3. Parse reference manual PDF → document chunks
4. Run `build_graph.py` → knowledge graph
5. Index into Qdrant → vector search
6. Update this file with the new chip mapping
