"""MCP Server for STM32F1 Embedded Documentation Query.

Exposes two tools to Claude Code:
- embedded_docs_query: Main RAG query with register-level precision
- embedded_docs_list_chips: List indexed chip models

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

app = Server("embedded-docs")
_orchestrator = None


def _get_orchestrator() -> RetrievalOrchestrator:
    global _orchestrator
    if _orchestrator is None:
        graph_path = os.path.join(plugin_root, "data", "graph", "f1_knowledge_graph.json")
        _orchestrator = RetrievalOrchestrator(graph_path=graph_path)
        _orchestrator.warm_up()
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
            description="List all STM32F1 chip models currently indexed in the knowledge base.",
            inputSchema={
                "type": "object",
                "properties": {},
            },
        ),
    ]


@app.call_tool()
async def call_tool(name: str, arguments: dict):
    if name == "embedded_docs_list_chips":
        return await handle_list_chips()

    if name == "embedded_docs_query":
        return await handle_query(arguments)

    raise ValueError(f"Unknown tool: {name}")


async def handle_list_chips() -> list[TextContent]:
    chips = [{
        "model": "STM32F103C8",
        "series": "STM32F1",
        "flash_kb": 64,
        "ram_kb": 20,
        "max_mhz": 72,
        "package": "LQFP48",
    }]
    return [TextContent(type="text", text=json.dumps(chips, indent=2))]


async def handle_query(arguments: dict) -> list[TextContent]:
    orch = _get_orchestrator()
    try:
        result = orch.query(
            query=arguments["query"],
            chip_model=arguments.get("chip_model", "STM32F103C8"),
            doc_revision=arguments.get("doc_revision", "Rev 21"),
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


async def main():
    _get_orchestrator()  # preload model to avoid cold start on first query
    async with stdio_server() as (read, write):
        await app.run(read, write, app.create_initialization_options())


if __name__ == "__main__":
    import asyncio
    asyncio.run(main())
