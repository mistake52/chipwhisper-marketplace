"""MCP Server for STM32 Serial Port Debugging.

Exposes 6 tools to Claude Code:
- serial_list_ports: Enumerate system serial ports
- serial_open: Open a serial connection
- serial_send: Send data over serial
- serial_read: Read data from serial
- serial_close: Close a serial connection
- get_usart_config: Query USART configuration from knowledge graph

Strict serialization: all port operations protected by a global lock.
flash (OpenOCD) and serial cannot run concurrently — close before flashing.
"""

import json
import threading
import uuid
from pathlib import Path

from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent

import serial
import serial.tools.list_ports

app = Server("serial-debug")
_sessions: dict[str, serial.Serial] = {}
_lock = threading.Lock()
_orchestrator = None


def _get_orchestrator():
    global _orchestrator
    if _orchestrator is None:
        try:
            from retrieval_orchestrator import RetrievalOrchestrator
            _orchestrator = RetrievalOrchestrator()
            _orchestrator.warm_up()
        except Exception:
            pass
    return _orchestrator


def _format_port_info(port) -> dict:
    info = {
        "port": port.device,
        "description": port.description,
        "hwid": port.hwid,
    }
    if port.vid is not None:
        info["vid"] = f"0x{port.vid:04X}"
    if port.pid is not None:
        info["pid"] = f"0x{port.pid:04X}"
    if port.serial_number:
        info["serial_number"] = port.serial_number
    if port.manufacturer:
        info["manufacturer"] = port.manufacturer
    return info


def _is_hex(data: str) -> bool:
    cleaned = data.replace(" ", "").replace("\n", "").replace("\r", "")
    if not cleaned:
        return False
    return all(c in "0123456789ABCDEFabcdef" for c in cleaned)


@app.list_tools()
async def list_tools() -> list[Tool]:
    return [
        Tool(
            name="serial_list_ports",
            description=(
                "List all available serial ports on this system. "
                "Use this FIRST before opening any connection — it shows which "
                "ports exist, their descriptions, and USB vendor/product IDs."
            ),
            inputSchema={"type": "object", "properties": {}},
        ),
        Tool(
            name="serial_open",
            description=(
                "Open a serial port connection. Returns a session_id for use with "
                "serial_send, serial_read, and serial_close. "
                "IMPORTANT: Only ONE serial operation runs at a time (strict serial mode). "
                "Before flashing firmware with OpenOCD, call serial_close first."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "port": {
                        "type": "string",
                        "description": "Serial port device path, e.g. /dev/ttyUSB0 or /dev/ttyACM0.",
                    },
                    "baudrate": {
                        "type": "integer",
                        "default": 115200,
                        "description": "Baud rate. Common values: 9600, 115200, 921600.",
                    },
                    "data_bits": {
                        "type": "integer",
                        "default": 8,
                        "enum": [5, 6, 7, 8],
                        "description": "Number of data bits per byte.",
                    },
                    "stop_bits": {
                        "type": "number",
                        "default": 1,
                        "enum": [1, 1.5, 2],
                        "description": "Number of stop bits.",
                    },
                    "parity": {
                        "type": "string",
                        "default": "N",
                        "enum": ["N", "E", "O", "M", "S"],
                        "description": "Parity: N=None, E=Even, O=Odd, M=Mark, S=Space.",
                    },
                    "flow_control": {
                        "type": "string",
                        "default": "none",
                        "enum": ["none", "rtscts", "xonxoff"],
                        "description": "Flow control mode.",
                    },
                    "timeout": {
                        "type": "number",
                        "default": 1.0,
                        "description": "Read timeout in seconds. Default 1.0s.",
                    },
                },
                "required": ["port"],
            },
        ),
        Tool(
            name="serial_send",
            description=(
                "Send data over an open serial connection. "
                "Data can be plain text (ASCII) or hex bytes (e.g. '48 65 6C 6C 6F'). "
                "Use \\r\\n for line endings if the device expects them."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "session_id": {
                        "type": "string",
                        "description": "Session ID returned by serial_open.",
                    },
                    "data": {
                        "type": "string",
                        "description": (
                            "Data to send. Plain text is sent as-is. "
                            "Hex format (space-separated bytes like '41 42 43') is "
                            "auto-detected and converted to raw bytes."
                        ),
                    },
                    "append_newline": {
                        "type": "boolean",
                        "default": False,
                        "description": "If true, append \\r\\n to the data.",
                    },
                },
                "required": ["session_id", "data"],
            },
        ),
        Tool(
            name="serial_read",
            description=(
                "Read data from an open serial connection. "
                "Blocks until data arrives or timeout expires. "
                "Returns data in both hex and ASCII formats."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "session_id": {
                        "type": "string",
                        "description": "Session ID returned by serial_open.",
                    },
                    "max_bytes": {
                        "type": "integer",
                        "default": 1024,
                        "description": "Maximum bytes to read. Default 1024.",
                    },
                    "timeout_ms": {
                        "type": "integer",
                        "default": 1000,
                        "description": "Read timeout in milliseconds. Default 1000ms.",
                    },
                },
                "required": ["session_id"],
            },
        ),
        Tool(
            name="serial_close",
            description=(
                "Close a serial port connection. Always call this when done — "
                "leaving a port open blocks flashing and other tools from using it. "
                "Safe to call even if already closed."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "session_id": {
                        "type": "string",
                        "description": "Session ID returned by serial_open.",
                    },
                },
                "required": ["session_id"],
            },
        ),
        Tool(
            name="get_usart_config",
            description=(
                "Query the STM32F1 knowledge base for USART peripheral configuration: "
                "baud rate register formula, TX/RX pin assignments, clock bus, and "
                "register base address. Use this before opening a serial connection "
                "to verify the correct USART peripheral and pins for your chip."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "chip_model": {
                        "type": "string",
                        "default": "STM32F103C8",
                        "description": "Chip model, e.g. STM32F103C8.",
                    },
                    "peripheral": {
                        "type": "string",
                        "default": "USART1",
                        "description": "USART peripheral name, e.g. USART1, USART2.",
                    },
                },
                "required": [],
            },
        ),
    ]


@app.call_tool()
async def call_tool(name: str, arguments: dict):
    handlers = {
        "serial_list_ports": handle_list_ports,
        "serial_open": handle_open,
        "serial_send": handle_send,
        "serial_read": handle_read,
        "serial_close": handle_close,
        "get_usart_config": handle_get_usart_config,
    }
    handler = handlers.get(name)
    if handler is None:
        raise ValueError(f"Unknown tool: {name}")
    return await handler(arguments)


async def handle_list_ports(_arguments: dict) -> list[TextContent]:
    ports = serial.tools.list_ports.comports()
    port_list = [_format_port_info(p) for p in ports]

    result = {
        "ports": port_list,
        "count": len(port_list),
        "permission_hint": (
            "If ports are missing, check: sudo usermod -a -G dialout $USER "
            "(then log out and back in)"
        ),
    }

    if not port_list:
        result["warning"] = (
            "No serial ports detected. If an STM32 board is connected, "
            "check the USB cable and try: dmesg | tail -20 | grep tty"
        )

    return [TextContent(type="text", text=json.dumps(result, indent=2, ensure_ascii=False))]


async def handle_open(arguments: dict) -> list[TextContent]:
    port = arguments["port"]
    baudrate = arguments.get("baudrate", 115200)
    data_bits = arguments.get("data_bits", 8)
    stop_bits = arguments.get("stop_bits", 1)
    parity = arguments.get("parity", "N")
    flow_control = arguments.get("flow_control", "none")
    timeout = arguments.get("timeout", 1.0)

    with _lock:
        try:
            ser = serial.Serial(
                port=port,
                baudrate=baudrate,
                bytesize=data_bits,
                stopbits=stop_bits,
                parity=parity,
                timeout=timeout,
                xonxoff=(flow_control == "xonxoff"),
                rtscts=(flow_control == "rtscts"),
            )
        except serial.SerialException as e:
            err_msg = str(e)
            available = [p.device for p in serial.tools.list_ports.comports()]
            hint = (
                "Permission denied — run: sudo usermod -a -G dialout $USER "
                "and log out/in"
                if "Permission" in err_msg or "denied" in err_msg.lower()
                else "Port not found or in use by another process"
            )
            return [TextContent(type="text", text=json.dumps({
                "status": "error",
                "error": err_msg,
                "hint": hint,
                "available_ports": available,
            }, indent=2))]

        session_id = uuid.uuid4().hex[:12]
        _sessions[session_id] = ser

    result = {
        "status": "connected",
        "port": port,
        "baudrate": baudrate,
        "data_bits": data_bits,
        "stop_bits": stop_bits,
        "parity": parity,
        "flow_control": flow_control,
        "session_id": session_id,
        "active_sessions": len(_sessions),
        "reminder": "Call serial_close when done to release the port for flashing.",
    }
    return [TextContent(type="text", text=json.dumps(result, indent=2))]


async def handle_send(arguments: dict) -> list[TextContent]:
    session_id = arguments["session_id"]
    data_str = arguments["data"]
    append_newline = arguments.get("append_newline", False)

    if append_newline:
        data_str += "\r\n"

    with _lock:
        ser = _sessions.get(session_id)
        if ser is None or not ser.is_open:
            return [TextContent(type="text", text=json.dumps({
                "status": "error",
                "error": f"Session {session_id} not found or port already closed.",
                "active_sessions": list(_sessions.keys()),
            }, indent=2))]

        if _is_hex(data_str):
            raw = bytes.fromhex(data_str.replace(" ", ""))
        else:
            raw = data_str.encode("utf-8")

        bytes_written = ser.write(raw)
        ser.flush()

    result = {
        "status": "sent",
        "bytes_written": bytes_written,
        "data_hex": raw.hex(" "),
        "data_ascii": data_str.replace("\r", "\\r").replace("\n", "\\n"),
    }
    return [TextContent(type="text", text=json.dumps(result, indent=2))]


async def handle_read(arguments: dict) -> list[TextContent]:
    session_id = arguments["session_id"]
    max_bytes = arguments.get("max_bytes", 1024)
    timeout_ms = arguments.get("timeout_ms", 1000)

    with _lock:
        ser = _sessions.get(session_id)
        if ser is None or not ser.is_open:
            return [TextContent(type="text", text=json.dumps({
                "status": "error",
                "error": f"Session {session_id} not found or port already closed.",
                "active_sessions": list(_sessions.keys()),
            }, indent=2))]

        original_timeout = ser.timeout
        ser.timeout = timeout_ms / 1000.0
        try:
            raw = ser.read(max_bytes)
        finally:
            ser.timeout = original_timeout

    readable = "".join(chr(b) if 32 <= b < 127 else "." for b in raw)
    result = {
        "status": "ok",
        "bytes_read": len(raw),
        "data_hex": raw.hex(" ") if raw else "",
        "data_ascii": readable,
        "timed_out": len(raw) == 0,
        "truncated": len(raw) >= max_bytes,
    }
    return [TextContent(type="text", text=json.dumps(result, indent=2))]


async def handle_close(arguments: dict) -> list[TextContent]:
    session_id = arguments["session_id"]

    with _lock:
        ser = _sessions.pop(session_id, None)
        if ser is None:
            return [TextContent(type="text", text=json.dumps({
                "status": "already_closed",
                "session_id": session_id,
            }, indent=2))]

        close_error = None
        try:
            ser.close()
        except Exception as e:
            close_error = str(e)

    result = {
        "status": "closed",
        "session_id": session_id,
        "remaining_sessions": len(_sessions),
        "ready_to_flash": len(_sessions) == 0,
    }
    if close_error:
        result["close_warning"] = close_error
    return [TextContent(type="text", text=json.dumps(result, indent=2))]


async def handle_get_usart_config(arguments: dict) -> list[TextContent]:
    chip = arguments.get("chip_model", "STM32F103C8")
    peripheral = arguments.get("peripheral", "USART1")

    orch = _get_orchestrator()
    if orch is None:
        return [TextContent(type="text", text=json.dumps({
            "status": "unavailable",
            "error": "Knowledge graph not loaded (requires embedded-docs MCP plugin).",
        }, indent=2))]

    try:
        result = orch.query(
            query=f"What are the USART configuration details for {peripheral} on {chip}?",
            chip_model=chip,
            intent_hint="config_guide",
            detail_level="standard",
            max_tokens=1024,
        )
    except Exception as e:
        return [TextContent(type="text", text=json.dumps({
            "status": "error",
            "error": str(e),
        }, indent=2))]

    return [TextContent(type="text", text=json.dumps(result, indent=2, ensure_ascii=False))]


async def main():
    async with stdio_server() as (read, write):
        await app.run(read, write, app.create_initialization_options())


if __name__ == "__main__":
    import asyncio
    asyncio.run(main())
