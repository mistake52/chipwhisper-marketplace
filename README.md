# ChipWhisper Marketplace

**STM32 bare-metal firmware development toolkit for Claude Code** — three MCP servers (documentation lookup, QEMU simulation, serial hardware debugging) plus a firmware development skill.

**STM32 裸机固件开发工具集**——三个 MCP 服务器（文档查询、QEMU 仿真、串口调试）加一个固件开发 Skill，专为 Claude Code 打造。

---

## Install / 安装

```bash
# 1. Add this marketplace
/plugin marketplace add mistake52/chipwhisper-marketplace

# 2. Install plugins (install all four for the full workflow)
/plugin install embedded-docs@chipwhisper-marketplace
/plugin install serial-debug@chipwhisper-marketplace
/plugin install simulation@chipwhisper-marketplace
/plugin install stm32-firmware-dev@chipwhisper-marketplace

# 3. Verify
claude mcp list
# embedded-docs  → ✓ Connected
# serial-debug   → ✓ Connected
# simulation     → ✓ Connected
```

## Plugins

| Plugin | Type | Description |
|--------|------|-------------|
| **embedded-docs** | MCP Server | Register-level STM32F1 documentation query — covers RCC, GPIO, USART, SPI, I2C, TIM, ADC, DMA, Flash, NVIC |
| **serial-debug** | MCP Server | Serial port debugging — list ports, open/close sessions, send/receive data via USB CDC ACM |
| **simulation** | MCP Server | QEMU firmware simulation — start/stop, GDB commands, register/memory inspection |
| **stm32-firmware-dev** | Skill | Complete firmware development workflow: docs → code → sim → hardware test |

## Prerequisites / 前置条件

| Plugin | Requires |
|--------|----------|
| **embedded-docs** | Qdrant running (`docker compose up -d`), ML model cached |
| **serial-debug** | Physical STM32 board or USB-UART adapter, `dialout` group permission |
| **simulation** | `qemu-system-arm`, `gdb-multiarch` (`sudo apt install qemu-system-arm gdb-multiarch`) |
| **stm32-firmware-dev** | All three MCP servers above |

Python dependencies are **auto-installed** by `uv` on first use — no manual `pip install` or `uv sync` needed.

## Usage / 使用

Once installed, the `stm32-firmware-dev` skill activates automatically when you mention STM32, embedded peripherals, or firmware development. You can also call the MCP tools directly:

```
# Documentation query
"STM32F103 的 USART1 状态寄存器 TXE 位怎么定义的？"

# Serial debugging  
"List serial ports and connect to /dev/ttyACM0 at 115200 baud"

# Simulation
"Start QEMU simulation with my firmware.elf for STM32F103C8"
```

For detailed workflow examples, see the [chipwhisper project](https://github.com/mistake52/chipwhisper).

## License

MIT
