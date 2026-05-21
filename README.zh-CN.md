# ChipWhisper Marketplace

**STM32 固件开发工具集**——三个 MCP 服务器（文档查询、QEMU 仿真、串口调试）加一个固件开发 Skill。从自然语言提问到图谱验证的 HAL 代码、仿真验证的固件、再到硬件实测——全流程覆盖。

[English README](README.md)

---

## 为什么用这个

写 STM32 固件意味着翻千页参考手册、交叉对照 SVD 文件、调试那些静默失败的寄存器配置——即使你用 HAL 库也不例外。ChipWhisper 让 Claude Code 帮你做这些重活：

- **寄存器级精度** — 不止告诉你"GPIOA 有 CRL 寄存器"。我们告诉你 CNF1=10（推挽输出）、MODE1=11（50MHz），并引用 RM0008 第 9.2.1 节第 172 页作为来源。每个寄存器值都可以追溯到参考手册。
- **图谱验证的 HAL 代码** — 知识图谱知道每个外设在哪个总线、每个引脚用什么复用功能。每一个 `__HAL_RCC_*_CLK_ENABLE()` 调用和每一个 `GPIO_InitTypeDef.Alternate` 值都有图谱提供的硬件真相作为依据。不会再出现"I2C 卡死，因为我忘了开 APB1 时钟"这种问题。
- **Phase -1 兼容性检查** — 写任何代码之前，Claude 会同时查询文档 MCP 和仿真 MCP。你得到一个 2×2 兼容性矩阵，清楚显示哪些已验证、哪些未验证。
- **仿真在上硬件前抓 bug** — QEMU 通过 GDB 验证时钟初始化、GPIO 配置和寄存器写入。不用碰物理板子就能捕获 PLL 挂起、总线错误和未开时钟的外设写入。
- **多芯片、纯配置驱动** — 添加新 STM32 芯片只需加一条 JSON 配置。整个流水线（下载→解析→建图→索引→查询）都是芯片型号参数化的。

---

## 插件一览

| 插件 | 类型 | 描述 |
|--------|------|-------------|
| **embedded-docs** | MCP Server | 寄存器级 STM32 文档查询。覆盖 RCC、GPIO、USART、SPI、I2C、TIM、ADC、DMA、Flash、NVIC。通过 `data/chips.json` 支持多芯片——为任意 STM32 系列添加 SVD 和参考手册即可。 |
| **serial-debug** | MCP Server | 串口调试——列出端口、打开/关闭会话、通过 USB CDC ACM 收发数据。 |
| **simulation** | MCP Server | QEMU 固件仿真——启动/停止、GDB 命令、寄存器/内存检查。支持 STM32F103 和 STM32F407。 |
| **stm32-firmware-dev** | Skill | 完整固件开发流程：兼容性检查→文档→编码→仿真→硬件测试。提及 STM32 或嵌入式任务时自动激活。 |

## 前置条件

| 插件 | 依赖 | 安装方法 |
|--------|----------|-------|
| **embedded-docs** | Qdrant + 知识库已构建 | 见下方 Qdrant 搭建和构建知识库 |
| **serial-debug** | 物理 STM32 开发板或 USB-UART 转接器 | `sudo usermod -aG dialout $USER` |
| **simulation** | `qemu-system-arm` + `gdb-multiarch` | `sudo apt install qemu-system-arm gdb-multiarch` |
| **stm32-firmware-dev** | 以上三个 MCP 服务器 | — |

Python 依赖由 `uv` **首次使用时自动安装**——无需手动 `pip install` 或 `uv sync`。

### Qdrant 搭建

`embedded-docs` 插件使用 Qdrant 进行向量搜索。启动一次后保持运行即可：

**方案 A：docker run（一行命令，无需克隆仓库）**
```bash
docker run -d --name qdrant-stm32 \
  -p 6333:6333 -p 6334:6334 \
  -v qdrant_storage:/qdrant/storage \
  --memory 512M --cpus 1.0 \
  --restart unless-stopped \
  qdrant/qdrant:latest
```

**方案 B：docker compose（需先克隆仓库）**
```bash
git clone https://github.com/mistake52/chipwhisper-marketplace.git
cd chipwhisper-marketplace
docker compose -f docker/docker-compose.yml up -d
```

验证 Qdrant 正常运行：
```bash
curl -s http://localhost:6333/healthz
```

## 安装

```bash
# 1. 添加此 Marketplace
/plugin marketplace add mistake52/chipwhisper-marketplace

# 2. 安装插件（建议四个全装以获得完整流程）
/plugin install embedded-docs@chipwhisper-marketplace
/plugin install serial-debug@chipwhisper-marketplace
/plugin install simulation@chipwhisper-marketplace
/plugin install stm32-firmware-dev@chipwhisper-marketplace

# 3. 验证
claude mcp list
# embedded-docs  → ✓ Connected
# serial-debug   → ✓ Connected
# simulation     → ✓ Connected
```

## 构建知识库

首次查询某个芯片前，需要构建知识图谱并索引到 Qdrant。SVD 文件已内置于插件中，PDF 首次运行时从 ST.com 下载。

**方案 A：在 Claude Code 内（推荐）**

安装插件后，直接让 Claude Code 执行构建流水线。Claude 会按顺序调用以下 MCP 工具：

| 步骤 | MCP 工具 | 作用 |
|------|----------|--------------|
| 1 | `embedded_docs_download` | 下载参考手册 + 数据手册 + 勘误表 PDF |
| 2 | `embedded_docs_parse` | 提取文本为结构化 Markdown |
| 3 | `embedded_docs_build_graph` | 从 SVD + 解析文档构建知识图谱 |
| 4 | `embedded_docs_index` | 文本嵌入并上传到 Qdrant |

直接说：*"为 STM32F103C8 构建知识库"*——Claude 会处理后续一切。

**方案 B：CLI（需要克隆仓库）**
```bash
cd chipwhisper-marketplace
uv run embedded-docs-build all --chip STM32F103C8
```

## 使用指南

`stm32-firmware-dev` Skill 在你提及 STM32、嵌入式外设或固件开发时自动激活。它遵循 5 个阶段的工作流：

### 完整工作流

```
Phase -1: 兼容性检查     ── 双 MCP 查询（文档+仿真可用性）
Phase  0: 文档调研       ── 从知识图谱获取寄存器值
Phase  1: 代码生成       ── HAL 库 C 代码（图谱验证时钟/引脚配置）
Phase  2: 仿真验证       ── QEMU + GDB 寄存器检查
Phase  3: 硬件测试       ── 烧录 + 串口输出捕获
```

核心差异点在于 **Phase -1**：写任何代码之前，Skill 同时查询 `embedded_docs_list_chips` 和 `list_supported_chips` 构建兼容性矩阵：

|           | 文档 MCP ✓ | 文档 MCP ✗ |
|-----------|:----------:|:----------:|
| **仿真 MCP ✓** | 全 MCP 验证（如 F103） | 仅仿真（如 F407） |
| **仿真 MCP ✗** | 仅文档验证 | 纯手工——警告用户 |

这让你清楚地知道哪些可以验证、哪些依赖通用知识——没有意外。

### 场景一：文档查询

```
"STM32F103 的 USART1 状态寄存器 TXE 位怎么定义的？"

→ embedded-docs 返回：TXE 是 USART1_SR（偏移 0x00）的第 7 位，
  硬件在 TDR 传输到移位寄存器后置位。
  来源：[RM0008 Rev 21, 27.6.1, P798]
```

### 场景二：固件开发

```
"为 STM32F103C8 写一个 MPU6050 的 I2C 驱动，用 PB6 (SCL) 和 PB7 (SDA)"

→ Skill 激活，运行 Phase -1 兼容性检查（F103 = 全 MCP 验证），
  查询 embedded-docs 获取 I2C1 引脚映射和寄存器配置，
  生成 HAL 库 C 代码 + Makefile + 链接脚本，
  可选在 QEMU 仿真中验证。
```

### 场景三：硬件调试

```
"连接 /dev/ttyACM0，115200 波特率，捕获传感器输出"

→ serial-debug 打开串口，读取数据，显示解析后的传感器值。
```

## 架构

### 知识图谱数据流

```
data/svd/<芯片>.svd ──[svd_parser]──▶ 外设/寄存器/位域节点
                                              │
data/raw/<文档>.pdf  ──[pymupdf]────▶ 解析后的 Markdown + 章节 JSON
                                              │
                                              ▼ [graph_builder]
                                       时钟节点 + 引脚节点 + 勘误节点
                                       + REQUIRES_CLOCK / LOCATED_AT_PIN 边
                                              │
Markdown 文本       ──[doc_chunker]──▶ DocumentChunk 节点（寄存器粒度）
                                              │
                                              ▼ [doc_associator]
                                       DESCRIBED_IN 边（L1 锚点 + L2 关联）
                                              │
                                              ▼
                                       data/graph/<图谱>.json  (~3500 节点, ~2200 边)
                                              │
                                              ▼ [index_docs]
                                       Qdrant 向量（384 维, all-MiniLM-L6-v2）
                                              │
                                              ▼ [retrieval_orchestrator]
                                       意图分类 → 图遍历 + 向量搜索 → 合并排序 → 答案
```

### 多芯片设计

所有芯片相关配置集中在一个文件——`data/chips.json`：

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

流水线中的每个模块都读取这份配置。添加新芯片只需加一条 JSON 条目——零 Python 代码改动。

## 添加新芯片

这需要编辑插件源码（fork 仓库或本地操作）：

```bash
# 1. 在 data/chips.json 中新增芯片条目，包含 SVD、参考手册下载链接、
#    时钟使能映射、总线拓扑、外设白名单和引脚映射。

# 2. 将 SVD 文件放入 data/svd/

# 3. 在 Claude Code 中运行流水线（Claude 会调用每个 MCP 工具）：
#    "为 STM32F407VG 构建知识库"
#    → 依次调用 embedded_docs_download → parse → build_graph → index

# 4. 查询新芯片：
#    "STM32F407VG 的 GPIO 端口模式寄存器偏移地址是多少？"

# 或通过 CLI：
cd chipwhisper-marketplace
uv run embedded-docs-build all --chip STM32F407VG
```

整个流水线完全由配置驱动——总线映射、时钟树、外设列表、文档元数据全都来自 `chips.json`。

## 致谢

本项目站在以下优秀项目的肩膀上：

| 项目 | 在 ChipWhisper 中的角色 |
|---------|---------------------|
| [cmsis-svd](https://github.com/posborne/cmsis-svd) | SVD 文件解析——从 ARM CMSIS-SVD XML 中提取每个寄存器和位域 |
| [NetworkX](https://networkx.org/) | 知识图谱构建——外设/寄存器/时钟/勘误节点和类型化边 |
| [Qdrant](https://qdrant.tech/) | 向量数据库——对 1600+ 参考手册文本块进行语义搜索 |
| [sentence-transformers](https://www.sbert.net/) | 文本嵌入——all-MiniLM-L6-v2 模型，适合 CPU 环境 |
| [PyMuPDF](https://pymupdf.readthedocs.io/) | PDF 文本提取——对原生数字 STM32 PDF 比 ML 解析器快约 1000 倍 |
| [QEMU](https://www.qemu.org/) | 固件仿真——在上硬件前捕获时钟挂起、总线错误和 GPIO 配置错误 |
| [Claude Code](https://claude.ai/code) | 让 MCP 服务器和 Skill 得以在 AI 辅助工作流中运行的平台 |

特别感谢 STM32 嵌入式开发社区，他们的参考手册、应用笔记和论坛讨论让寄存器级固件开发成为可能。

## License

MIT
