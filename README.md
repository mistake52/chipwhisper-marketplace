# ChipWhisper Marketplace

**STM32 bare-metal firmware development toolkit for Claude Code** — three MCP servers (documentation lookup, QEMU simulation, serial hardware debugging) plus a firmware development skill. From a natural language query to verified register values, simulated firmware, and hardware-tested code.

[中文文档 (Chinese README)](README.zh-CN.md)

---

## Why ChipWhisper

Writing STM32 bare-metal firmware means navigating 1000-page reference manuals, cross-referencing SVD files, and debugging register configs that silently fail. ChipWhisper lets Claude Code do the heavy lifting:

- **Register-level precision** — Not just "GPIOA has a CRL register." We tell you CNF1=10 (push-pull), MODE1=11 (50MHz), and cite RM0008 Section 9.2.1, page 172 as the source. Every register value is traceable to the reference manual.
- **Phase -1 compatibility check** — Before writing a single line of code, Claude queries BOTH the docs MCP and simulation MCP. You get a 2×2 compatibility matrix showing exactly what's verified and what's not.
- **Simulation catches bugs before hardware** — QEMU verifies clock initialization, GPIO configs, and register writes via GDB. Catches PLL hangs, bus faults, and unclocked peripheral writes without touching a physical board.
- **Multi-chip, config-driven** — Adding a new STM32 chip means adding one JSON entry. The entire pipeline (download → parse → build graph → index → query) is parameterized by chip model.

---

## Plugins

| Plugin | Type | Description |
|--------|------|-------------|
| **embedded-docs** | MCP Server | Register-level STM32 documentation query. Covers RCC, GPIO, USART, SPI, I2C, TIM, ADC, DMA, Flash, NVIC. Multi-chip via `data/chips.json` — add an SVD + reference manual for any STM32 family. |
| **serial-debug** | MCP Server | Serial port debugging — list ports, open/close sessions, send/receive data via USB CDC ACM. |
| **simulation** | MCP Server | QEMU firmware simulation — start/stop, GDB commands, register/memory inspection. Supports STM32F103 and STM32F407. |
| **stm32-firmware-dev** | Skill | Complete firmware development workflow: compatibility check → docs → code → sim → hardware test. Activates automatically on any STM32/embedded task. |

## Prerequisites

| Plugin | Requires | Setup |
|--------|----------|-------|
| **embedded-docs** | Qdrant + ML model | See Qdrant setup below |
| **serial-debug** | Physical STM32 board or USB-UART adapter | `sudo usermod -aG dialout $USER` |
| **simulation** | `qemu-system-arm` + `gdb-multiarch` | `sudo apt install qemu-system-arm gdb-multiarch` |
| **stm32-firmware-dev** | All three MCP servers above | — |

Python dependencies are **auto-installed** by `uv` on first use — no manual `pip install` or `uv sync` needed.

### Qdrant Setup

The `embedded-docs` plugin uses Qdrant for vector search over reference manual chunks. Start it once and leave it running:

```bash
# Clone the repo if you need the docker-compose file
git clone https://github.com/mistake52/chipwhisper-marketplace.git
cd chipwhisper-marketplace

# Start Qdrant (512MB / 1CPU limit, port 6333)
docker compose -f docker/docker-compose.yml up -d

# Verify it's healthy
curl -s http://localhost:6333/healthz
```

### Build the Knowledge Base

Before querying a chip for the first time, build its knowledge graph and index it into Qdrant via MCP tools inside Claude Code (or via CLI):

```
# Inside Claude Code
embedded_docs_download  → downloads reference manual + datasheet + errata PDFs
embedded_docs_parse     → extracts text into structured Markdown
embedded_docs_build_graph → builds knowledge graph from SVD + parsed docs
embedded_docs_index     → embeds chunks and uploads to Qdrant

# Or use the CLI
uv run embedded-docs-build all --chip STM32F103C8
```

The SVD file is bundled with the plugin. PDFs are downloaded from ST.com on first run.

## Install

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

## Usage

The `stm32-firmware-dev` skill activates automatically when you mention STM32, embedded peripherals, or firmware development. It follows a 5-phase workflow:

### Complete Workflow

```
Phase -1: Compatibility Check   ── Dual MCP query (docs + sim availability)
Phase  0: Documentation Research ── Register values from knowledge graph
Phase  1: Code Generation        ── Bare-metal C with register-level access
Phase  2: Simulation Verification── QEMU + GDB register inspection
Phase  3: Hardware Testing       ── Flash + serial output capture
```

The key differentiator is **Phase -1**: before writing any code, the skill queries both `embedded_docs_list_chips` and `list_supported_chips` to build a compatibility matrix:

|           | Docs MCP ✓ | Docs MCP ✗ |
|-----------|:----------:|:----------:|
| **Sim MCP ✓** | FULL MCP (e.g. F103) | Sim-only (e.g. F407) |
| **Sim MCP ✗** | Docs-only | MANUAL ONLY — warns user |

This tells you exactly what can be verified vs. what relies on general knowledge — no surprises.

### Scenario 1: Documentation Query

```
"STM32F103 的 USART1 状态寄存器 TXE 位怎么定义的？"

→ embedded-docs returns: TXE is bit 7 of USART1_SR (offset 0x00),
  set by hardware when TDR is transferred to shift register.
  Source: [RM0008 Rev 21, 27.6.1, P798]
```

### Scenario 2: Firmware Development

```
"Write an I2C driver for MPU6050 on STM32F103C8, using PB6 (SCL) and PB7 (SDA)"

→ Skill activates, runs Phase -1 compatibility check (F103 = FULL MCP),
  queries embedded-docs for I2C1 pin mapping + register config,
  generates bare-metal C with Makefile + linker script,
  optionally verifies in QEMU simulation.
```

### Scenario 3: Hardware Debug

```
"Connect to /dev/ttyACM0 at 115200 and capture sensor output"

→ serial-debug opens the port, reads data, displays parsed sensor values.
```

## Architecture

### Knowledge Graph Data Flow

```
data/svd/<chip>.svd ──[svd_parser]──▶ Peripheral/Register/BitField nodes
                                              │
data/raw/<doc>.pdf   ──[pymupdf]────▶ parsed Markdown + sections JSON
                                              │
                                              ▼ [graph_builder]
                                       Clock nodes + Pin nodes + Errata nodes
                                       + REQUIRES_CLOCK / LOCATED_AT_PIN edges
                                              │
parsed Markdown      ──[doc_chunker]──▶ DocumentChunk nodes (register granularity)
                                              │
                                              ▼ [doc_associator]
                                       DESCRIBED_IN edges (L1 anchors + L2 associations)
                                              │
                                              ▼
                                       data/graph/<graph>.json  (~3500 nodes, ~2200 edges)
                                              │
                                              ▼ [index_docs]
                                       Qdrant vectors (384-dim, all-MiniLM-L6-v2)
                                              │
                                              ▼ [retrieval_orchestrator]
                                       Intent classification → graph traversal
                                       + vector search → merge & rank → answer
```

### Multi-Chip Design

All chip-specific configuration lives in a single file — `data/chips.json`:

```json
{
  "STM32F103C8": {
    "series": "STM32F1",
    "node_prefix": "F103",
    "svd": "STM32F103xx.svd",
    "graph": "f1_knowledge_graph.json",
    "qdrant_collection": "stm32f1_docs",
    "clock_enable_map": { "GPIOA": ["APB2", "RCC_APB2ENR", 2, "0x40021018"], ... },
    "bus_map": { "APB1": [...], "APB2": [...], "AHB": [...] },
    "peripheral_whitelist": ["RCC", "GPIOA", ...],
    ...
  }
}
```

Every module in the pipeline reads from this config. Adding a new chip means adding one JSON entry — zero Python changes.

## Adding a New Chip

```bash
# 1. Add an entry to data/chips.json with the chip's SVD, reference manual URL,
#    clock enable map, bus topology, peripheral whitelist, and pin mapping.

# 2. Place the SVD file in data/svd/

# 3. Inside Claude Code, run the pipeline:
embedded_docs_download --chip STM32F407VG
embedded_docs_parse --chip STM32F407VG
embedded_docs_build_graph --chip STM32F407VG
embedded_docs_index --chip STM32F407VG

# 4. Query the new chip:
embedded_docs_query --chip STM32F407VG "What are the GPIO port mode register offsets?"

# Or via CLI:
uv run embedded-docs-build all --chip STM32F407VG
```

The pipeline is fully config-driven — bus maps, clock trees, peripheral lists, and document metadata all come from `chips.json`.

## Acknowledgments

This project stands on the shoulders of excellent open-source tools:

| Project | Role in ChipWhisper |
|---------|---------------------|
| [cmsis-svd](https://github.com/posborne/cmsis-svd) | SVD file parsing — extracts every register and bitfield from ARM CMSIS-SVD XML |
| [NetworkX](https://networkx.org/) | Knowledge graph construction — peripheral/register/clock/errata nodes and typed edges |
| [Qdrant](https://qdrant.tech/) | Vector database — semantic search over 1600+ reference manual chunks |
| [sentence-transformers](https://www.sbert.net/) | Text embeddings — all-MiniLM-L6-v2 for CPU-friendly semantic search |
| [PyMuPDF](https://pymupdf.readthedocs.io/) | PDF text extraction — ~1000× faster than ML-based parsers for born-digital STM32 PDFs |
| [QEMU](https://www.qemu.org/) | Firmware simulation — catches clock hangs, bus faults, and GPIO config errors before hardware |
| [Claude Code](https://claude.ai/code) | The platform that makes MCP servers and skills possible in an AI-assisted workflow |

Special thanks to the STM32 bare-metal community whose reference manuals, application notes, and forum discussions make register-level firmware development possible.

## License

MIT
