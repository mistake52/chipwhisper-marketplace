"""MCP Server for STM32 Embedded Documentation Query.

Exposes 6 tools to Claude Code:
- embedded_docs_query:      Main RAG query with register-level precision
- embedded_docs_list_chips: List available chip models from the registry
- embedded_docs_download:   Download reference PDFs from ST.com
- embedded_docs_parse:      Parse PDFs into structured Markdown (pymupdf)
- embedded_docs_build_graph: Build knowledge graph from SVD + parsed markdown
- embedded_docs_index:      Embed chunks and upload to Qdrant

Uses stdio transport for Claude Code MCP integration.
"""

import json
import os
import sys
from pathlib import Path

# CLAUDE_PLUGIN_ROOT is set by Claude Code when running as a plugin MCP server
plugin_root = os.environ.get("CLAUDE_PLUGIN_ROOT", ".")
sys.path.insert(0, plugin_root)

from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent

from src.retrieval.retrieval_orchestrator import RetrievalOrchestrator
from src.retrieval.intent_classifier import INTENT_TYPES
from embedded_docs import build, chip_registry

app = Server("embedded-docs")
_orchestrator = None
_orchestrator_chip: str | None = None
DATA_DIR = Path(plugin_root) / "data"


def _get_orchestrator(chip_model: str = "STM32F103C8") -> RetrievalOrchestrator:
    global _orchestrator, _orchestrator_chip
    if _orchestrator is not None and _orchestrator_chip == chip_model:
        return _orchestrator

    cfg = chip_registry.get_chip_config(chip_model, data_dir=DATA_DIR)
    graph_path = DATA_DIR / "graph" / cfg["graph"]
    _orchestrator = RetrievalOrchestrator(graph_path=str(graph_path), chip_config=cfg)
    _orchestrator.warm_up()
    _orchestrator_chip = chip_model
    return _orchestrator


@app.list_tools()
async def list_tools() -> list[Tool]:
    return [
        Tool(
            name="embedded_docs_query",
            description=(
                "Query STM32F1 technical documentation with register-level precision. "
                "Use this for any question about STM32F1 peripherals, registers, bitfields, "
                "clock configuration, pin mapping, or errata."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": (
                            "Natural language query in English. "
                            "If the user asked in Chinese (中文) or any other language, "
                            "translate their question to English before calling this tool. "
                            "STM32 register names (USART1_SR, RCC_APB2ENR), peripheral names "
                            "(GPIOA, TIM2, ADC1), and hex addresses (0x40021018) must be "
                            "preserved exactly — do not translate them."
                        ),
                    },
                    "chip_model": {
                        "type": "string",
                        "description": "Specific chip model, e.g. 'STM32F103C8'. Defaults to STM32F103C8.",
                    },
                    "doc_revision": {
                        "type": "string",
                        "description": "Reference manual revision, e.g. 'Rev 21'. If omitted, uses latest.",
                    },
                    "intent_hint": {
                        "type": "string",
                        "enum": ["auto"] + INTENT_TYPES,
                        "description": "Optional hint for retrieval strategy. 'auto' lets the system classify.",
                    },
                    "detail_level": {
                        "type": "string",
                        "enum": ["compact", "standard", "verbose"],
                        "default": "standard",
                        "description": "Controls context verbosity to fit within token budget.",
                    },
                    "max_tokens": {
                        "type": "integer",
                        "default": 2048,
                        "description": "Hard token budget for returned context.",
                    },
                },
                "required": ["query"],
            },
        ),
        Tool(
            name="embedded_docs_list_chips",
            description="List all chip models available in the registry (data/chips.json).",
            inputSchema={
                "type": "object",
                "properties": {},
            },
        ),
        Tool(
            name="embedded_docs_download",
            description=(
                "Download reference PDFs (reference manual, datasheet, errata) from ST.com "
                "for the specified chip model. The SVD file is already bundled in the plugin. "
                "Downloads are skipped if files already exist. Use force=true to re-download."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "chip_model": {
                        "type": "string",
                        "description": "Chip model to download docs for, e.g. 'STM32F103C8'. Defaults to STM32F103C8.",
                    },
                    "force": {
                        "type": "boolean",
                        "description": "Re-download even if files already exist.",
                        "default": False,
                    },
                },
            },
        ),
        Tool(
            name="embedded_docs_parse",
            description=(
                "Parse downloaded PDFs into structured Markdown using pymupdf. "
                "Outputs go to data/parsed/<doc_id>/. Skipped if output already exists. "
                "Use force=true to re-parse."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "chip_model": {
                        "type": "string",
                        "description": "Chip model to parse docs for, e.g. 'STM32F103C8'. Defaults to STM32F103C8.",
                    },
                    "force": {
                        "type": "boolean",
                        "description": "Re-parse even if output already exists.",
                        "default": False,
                    },
                },
            },
        ),
        Tool(
            name="embedded_docs_build_graph",
            description=(
                "Build the knowledge graph from SVD + parsed reference manual markdown "
                "for the specified chip model. Produces a graph JSON, chunks JSON, and "
                "an association report. Requires: parsed markdown (run parse step first). "
                "Skipped if graph already exists. Use force=true to rebuild."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "chip_model": {
                        "type": "string",
                        "description": "Chip model to build graph for, e.g. 'STM32F103C8'. Defaults to STM32F103C8.",
                    },
                    "force": {
                        "type": "boolean",
                        "description": "Rebuild even if graph already exists.",
                        "default": False,
                    },
                },
            },
        ),
        Tool(
            name="embedded_docs_index",
            description=(
                "Embed document chunks using all-MiniLM-L6-v2 and upload to Qdrant "
                "for vector search. Requires: Qdrant running on localhost:6333, "
                "chunks JSON (run build-graph step first). "
                "Skipped if collection already has data. Use force=true to re-index."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "chip_model": {
                        "type": "string",
                        "description": "Chip model to index docs for, e.g. 'STM32F103C8'. Defaults to STM32F103C8.",
                    },
                    "force": {
                        "type": "boolean",
                        "description": "Re-index even if collection already has data.",
                        "default": False,
                    },
                },
            },
        ),
    ]


@app.call_tool()
async def call_tool(name: str, arguments: dict):
    if name == "embedded_docs_list_chips":
        return await handle_list_chips()

    if name == "embedded_docs_query":
        return await handle_query(arguments)

    if name == "embedded_docs_download":
        return await _run_build_step("download", arguments)

    if name == "embedded_docs_parse":
        return await _run_build_step("parse", arguments)

    if name == "embedded_docs_build_graph":
        return await _run_build_step("build-graph", arguments)

    if name == "embedded_docs_index":
        return await _run_build_step("index", arguments)

    raise ValueError(f"Unknown tool: {name}")


async def handle_list_chips() -> list[TextContent]:
    models = chip_registry.list_chip_models(data_dir=DATA_DIR)
    chips = []
    for model in models:
        try:
            cfg = chip_registry.get_chip_config(model, data_dir=DATA_DIR)
            spec = cfg.get("chip_spec", {})
            chips.append({
                "model": model,
                "series": cfg.get("series", ""),
                "flash_kb": spec.get("flash_kb"),
                "ram_kb": spec.get("ram_kb"),
                "max_mhz": spec.get("max_mhz"),
                "package": spec.get("package"),
            })
        except Exception:
            chips.append({"model": model, "series": "?"})
    return [TextContent(type="text", text=json.dumps(chips, indent=2))]


async def handle_query(arguments: dict) -> list[TextContent]:
    chip_model = arguments.get("chip_model", "STM32F103C8")
    orch = _get_orchestrator(chip_model)
    cfg = chip_registry.get_chip_config(chip_model, data_dir=DATA_DIR)
    default_rev = cfg.get("reference_manual", {}).get("revision", "Rev 21")
    try:
        result = orch.query(
            query=arguments["query"],
            chip_model=chip_model,
            doc_revision=arguments.get("doc_revision", default_rev),
            intent_hint=arguments.get("intent_hint", "auto"),
            detail_level=arguments.get("detail_level", "standard"),
            max_tokens=arguments.get("max_tokens", 2048),
        )
    except Exception as e:
        return [TextContent(
            type="text",
            text=json.dumps({"error": str(e), "answer_context": f"Retrieval error: {e}"}, indent=2),
        )]

    return [TextContent(type="text", text=json.dumps(result, indent=2, ensure_ascii=False))]


async def _run_build_step(step: str, arguments: dict) -> list[TextContent]:
    """Run a build pipeline step from the build module."""
    chip_model = arguments.get("chip_model", "STM32F103C8")
    force = arguments.get("force", False)
    step_fn = {
        "download": build.download_docs,
        "parse": build.parse_pdfs,
        "build-graph": build.build_graph,
        "index": build.index_chunks,
    }[step]
    try:
        result = step_fn(chip_model=chip_model, force=force)
    except Exception as e:
        result = {"ok": False, "error": str(e)}
    return [TextContent(type="text", text=json.dumps(result, indent=2, ensure_ascii=False))]


async def main():
    _get_orchestrator()  # preload model to avoid cold start on first query
    async with stdio_server() as (read, write):
        await app.run(read, write, app.create_initialization_options())


if __name__ == "__main__":
    import asyncio
    asyncio.run(main())
