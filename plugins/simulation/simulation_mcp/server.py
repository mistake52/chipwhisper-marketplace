"""MCP Server for STM32 QEMU Firmware Simulation.

Exposes 5 tools to Claude Code:
- list_supported_chips: Show which STM32 models QEMU can emulate
- start_sim: Launch QEMU with an ELF firmware file
- stop_sim: Terminate a running simulation
- get_status: Query simulation state via QEMU monitor
- send_gdb_cmd: Send GDB commands to inspect CPU/memory

Manages QEMU subprocesses with TCP-based monitor and GDB stub ports.
"""

import json
import os
import shutil
import signal
import socket
import subprocess
import threading
import time

from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent

app = Server("simulation")

# {pid: {"proc": Popen, "monitor_port": int, "gdb_port": int,
#         "chip_model": str, "elf_file": str, "started_at": float}}
_sims: dict[int, dict] = {}
_lock = threading.Lock()
_next_gdb_port = 3333
_next_monitor_port = 4444

SUPPORTED_CHIPS = {
    "STM32F103C8": {
        "qemu_machine": "stm32vldiscovery",
        "cpu": "cortex-m3",
        "flash_kb": 64,
        "ram_kb": 20,
        "max_mhz": 72,
        "note": "STM32VLDISCOVERY uses STM32F100RBT6 (Cortex-M3, Value Line). Register map is subset of F103 — basic peripherals compatible.",
    },
    "STM32F103RBT6": {
        "qemu_machine": "stm32vldiscovery",
        "cpu": "cortex-m3",
        "flash_kb": 128,
        "ram_kb": 20,
        "max_mhz": 72,
        "note": "Closest Cortex-M3 match. STM32F100 emulation may lack F103-specific peripherals.",
    },
    "STM32F407VG": {
        "qemu_machine": "netduinoplus2",
        "cpu": "cortex-m4",
        "flash_kb": 1024,
        "ram_kb": 128,
        "max_mhz": 168,
    },
    "STM32F405RG": {
        "qemu_machine": "olimex-stm32-h405",
        "cpu": "cortex-m4",
        "flash_kb": 1024,
        "ram_kb": 128,
        "max_mhz": 168,
    },
}


def _check_qemu() -> str | None:
    """Return None if qemu-system-arm is available, else error message."""
    if shutil.which("qemu-system-arm") is None:
        return "qemu-system-arm not found. Install: sudo apt install qemu-system-arm"
    return None


def _check_gdb() -> str | None:
    """Return None if gdb-multiarch is available, else warning message."""
    if shutil.which("gdb-multiarch") is None:
        return "gdb-multiarch not found. GDB features unavailable. Install: sudo apt install gdb-multiarch"
    return None


def _find_free_port(start: int) -> int:
    """Find a free TCP port starting from `start`."""
    port = start
    for _ in range(100):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind(("127.0.0.1", port))
                return port
            except OSError:
                port += 1
    raise RuntimeError("No free TCP ports in range")


def _send_monitor_cmd(monitor_port: int, cmd: str, timeout: float = 3.0) -> str:
    """Send a command to QEMU monitor over TCP and return the response."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect(("127.0.0.1", monitor_port))
        try:
            sock.recv(1024)
        except socket.timeout:
            pass
        sock.sendall((cmd + "\n").encode())
        time.sleep(0.2)
        response = b""
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
                if b"\n(qemu)" in response:
                    break
            except socket.timeout:
                break
        text = response.decode("utf-8", errors="replace")
        text = text.replace("\r", "")
        lines = text.split("\n")
        lines = [l for l in lines if "(qemu)" not in l]
        return "\n".join(lines).strip()
    except (socket.timeout, ConnectionRefusedError, OSError) as e:
        return f"[monitor error: {e}]"
    finally:
        sock.close()


def _cleanup_dead():
    """Remove sim entries whose QEMU process has exited."""
    dead = []
    for pid, info in _sims.items():
        proc = info["proc"]
        if proc.poll() is not None:
            dead.append(pid)
    for pid in dead:
        del _sims[pid]


@app.list_tools()
async def list_tools() -> list[Tool]:
    return [
        Tool(
            name="list_supported_chips",
            description=(
                "List STM32 chip models that QEMU can emulate. "
                "Each entry shows the QEMU machine name, CPU core, and memory sizes."
            ),
            inputSchema={"type": "object", "properties": {}},
        ),
        Tool(
            name="start_sim",
            description=(
                "Start a QEMU simulation with an ELF firmware file. "
                "QEMU starts paused (-S) waiting for GDB to connect. "
                "Returns a PID for use with stop_sim, get_status, send_gdb_cmd."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "elf_file": {
                        "type": "string",
                        "description": "Absolute path to the ELF firmware file.",
                    },
                    "chip_model": {
                        "type": "string",
                        "default": "STM32F103C8",
                        "description": "Chip model. Use list_supported_chips to see options.",
                    },
                    "gdb_port": {
                        "type": "integer",
                        "description": "TCP port for GDB stub. Auto-assigned if omitted.",
                    },
                    "extra_qemu_args": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "Additional QEMU CLI arguments, e.g. ['-d', 'cpu,int'].",
                    },
                },
                "required": ["elf_file"],
            },
        ),
        Tool(
            name="stop_sim",
            description=(
                "Stop a running QEMU simulation. Sends SIGTERM first, then SIGKILL "
                "if the process does not exit within 3 seconds."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "pid": {
                        "type": "integer",
                        "description": "Process ID returned by start_sim.",
                    },
                },
                "required": ["pid"],
            },
        ),
        Tool(
            name="get_status",
            description=(
                "Query the status of a running simulation: whether QEMU is alive, "
                "current PC value, elapsed time, and briefly checks the monitor socket."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "pid": {
                        "type": "integer",
                        "description": "Process ID returned by start_sim.",
                    },
                },
                "required": ["pid"],
            },
        ),
        Tool(
            name="send_gdb_cmd",
            description=(
                "Send a GDB command to a running simulation. "
                "Uses gdb-multiarch in batch mode via TCP to the QEMU GDB stub. "
                "Examples: 'info registers', 'x/10x 0x08000000', 'monitor info mtree'."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "pid": {
                        "type": "integer",
                        "description": "Process ID returned by start_sim.",
                    },
                    "command": {
                        "type": "string",
                        "description": "GDB command to execute, e.g. 'info registers'.",
                    },
                },
                "required": ["pid", "command"],
            },
        ),
    ]


@app.call_tool()
async def call_tool(name: str, arguments: dict):
    handlers = {
        "list_supported_chips": handle_list_chips,
        "start_sim": handle_start_sim,
        "stop_sim": handle_stop_sim,
        "get_status": handle_get_status,
        "send_gdb_cmd": handle_send_gdb_cmd,
    }
    handler = handlers.get(name)
    if handler is None:
        raise ValueError(f"Unknown tool: {name}")
    return await handler(arguments)


async def handle_list_chips(_arguments: dict) -> list[TextContent]:
    qemu_err = _check_qemu()
    chips = []
    for model, info in SUPPORTED_CHIPS.items():
        chips.append({"model": model, **info})

    result = {
        "chips": chips,
        "count": len(chips),
        "qemu_available": qemu_err is None,
        "qemu_error": qemu_err,
    }
    return [TextContent(type="text", text=json.dumps(result, indent=2))]


async def handle_start_sim(arguments: dict) -> list[TextContent]:
    elf_file = arguments["elf_file"]
    chip_model = arguments.get("chip_model", "STM32F103C8")

    qemu_err = _check_qemu()
    if qemu_err:
        return [TextContent(type="text", text=json.dumps({
            "status": "error", "error": qemu_err,
        }, indent=2))]

    if not os.path.isfile(elf_file):
        return [TextContent(type="text", text=json.dumps({
            "status": "error",
            "error": f"ELF file not found: {elf_file}",
        }, indent=2))]

    chip_info = SUPPORTED_CHIPS.get(chip_model)
    if chip_info is None:
        known = list(SUPPORTED_CHIPS.keys())
        return [TextContent(type="text", text=json.dumps({
            "status": "error",
            "error": f"Unknown chip model: {chip_model}",
            "supported_models": known,
        }, indent=2))]

    global _next_gdb_port, _next_monitor_port

    MAX_PORT_RETRIES = 5

    with _lock:
        _cleanup_dead()

        gdb_port = arguments.get("gdb_port")
        monitor_port = None

        for attempt in range(MAX_PORT_RETRIES):
            if gdb_port is None:
                gdb_port = _find_free_port(_next_gdb_port)
                _next_gdb_port = gdb_port + 1
            monitor_port = _find_free_port(_next_monitor_port)
            _next_monitor_port = monitor_port + 1

            cmd = [
                "qemu-system-arm",
                "-M", chip_info["qemu_machine"],
                "-kernel", elf_file,
                "-nographic",
                "-monitor", f"tcp:127.0.0.1:{monitor_port},server,nowait",
                "-gdb", f"tcp:127.0.0.1:{gdb_port}",
                "-S",
            ]
            extra = arguments.get("extra_qemu_args", [])
            if extra:
                cmd.extend(extra)

            try:
                proc = subprocess.Popen(
                    cmd,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
            except OSError as e:
                return [TextContent(type="text", text=json.dumps({
                    "status": "error",
                    "error": f"Failed to start QEMU: {e}",
                }, indent=2))]

            time.sleep(0.1)
            if proc.poll() is not None:
                stderr = proc.stderr.read().decode(errors="replace")
                if "address already in use" in stderr.lower():
                    gdb_port = None
                    continue
                return [TextContent(type="text", text=json.dumps({
                    "status": "error",
                    "error": f"QEMU exited immediately: {stderr[:200]}",
                }, indent=2))]
            break
        else:
            return [TextContent(type="text", text=json.dumps({
                "status": "error",
                "error": f"Failed to allocate free ports after {MAX_PORT_RETRIES} attempts.",
            }, indent=2))]

        pid = proc.pid
        _sims[pid] = {
            "proc": proc,
            "monitor_port": monitor_port,
            "gdb_port": gdb_port,
            "chip_model": chip_model,
            "elf_file": elf_file,
            "started_at": time.time(),
        }

    gdb_warning = _check_gdb()

    result = {
        "status": "started",
        "pid": pid,
        "chip_model": chip_model,
        "qemu_machine": chip_info["qemu_machine"],
        "elf_file": elf_file,
        "gdb_port": gdb_port,
        "monitor_port": monitor_port,
        "command_line": " ".join(cmd),
        "note": "QEMU started with -S (paused). Use send_gdb_cmd with 'continue' to run.",
        "gdb_warning": gdb_warning,
    }
    return [TextContent(type="text", text=json.dumps(result, indent=2))]


async def handle_stop_sim(arguments: dict) -> list[TextContent]:
    pid = arguments["pid"]

    with _lock:
        _cleanup_dead()
        info = _sims.pop(pid, None)

    if info is None:
        return [TextContent(type="text", text=json.dumps({
            "status": "not_found",
            "pid": pid,
            "message": "No simulation with this PID (already stopped or never started).",
            "active_simulations": len(_sims),
        }, indent=2))]

    proc = info["proc"]
    exit_code = None
    error = None
    try:
        proc.terminate()
        try:
            exit_code = proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            try:
                exit_code = proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                error = "Process did not exit after SIGKILL (may be in D state)"
    except ProcessLookupError:
        exit_code = proc.poll()

    result = {
        "status": "stopped",
        "pid": pid,
        "exit_code": exit_code,
        "was_running": exit_code is not None,
        "chip_model": info["chip_model"],
        "remaining_simulations": len(_sims),
    }
    if error:
        result["error"] = error
    return [TextContent(type="text", text=json.dumps(result, indent=2))]


async def handle_get_status(arguments: dict) -> list[TextContent]:
    pid = arguments["pid"]

    with _lock:
        _cleanup_dead()
        info = _sims.get(pid)

    if info is None:
        return [TextContent(type="text", text=json.dumps({
            "status": "not_found",
            "pid": pid,
            "active_simulations": list(_sims.keys()),
        }, indent=2))]

    proc = info["proc"]
    poll = proc.poll()
    running = poll is None
    uptime = time.time() - info["started_at"]

    pc_value = None
    if running:
        pc_text = _send_monitor_cmd(info["monitor_port"], "info registers")
        for line in pc_text.split("\n"):
            if "pc=" in line.lower() or "R15" in line or "PC" in line:
                pc_value = line.strip()
                break
        if not pc_value:
            pc_value = pc_text[:200] if pc_text else "(no response)"

    result = {
        "status": "running" if running else "exited",
        "pid": pid,
        "exit_code": poll if not running else None,
        "chip_model": info["chip_model"],
        "elf_file": info["elf_file"],
        "gdb_port": info["gdb_port"],
        "monitor_port": info["monitor_port"],
        "uptime_sec": round(uptime, 1),
        "pc_value": pc_value,
    }
    return [TextContent(type="text", text=json.dumps(result, indent=2))]


async def handle_send_gdb_cmd(arguments: dict) -> list[TextContent]:
    pid = arguments["pid"]
    command = arguments["command"]

    gdb_err = _check_gdb()
    if gdb_err:
        return [TextContent(type="text", text=json.dumps({
            "status": "error", "error": gdb_err,
        }, indent=2))]

    with _lock:
        _cleanup_dead()
        info = _sims.get(pid)

    if info is None:
        return [TextContent(type="text", text=json.dumps({
            "status": "not_found",
            "pid": pid,
            "active_simulations": list(_sims.keys()),
        }, indent=2))]

    proc = info["proc"]
    if proc.poll() is not None:
        return [TextContent(type="text", text=json.dumps({
            "status": "error",
            "error": f"Simulation (PID {pid}) has exited with code {proc.returncode}.",
        }, indent=2))]

    gdb_port = info["gdb_port"]

    try:
        result = subprocess.run(
            ["gdb-multiarch", "-batch", "-nx",
             "-ex", f"target remote :{gdb_port}",
             "-ex", command],
            capture_output=True,
            text=True,
            timeout=10,
        )
    except subprocess.TimeoutExpired:
        return [TextContent(type="text", text=json.dumps({
            "status": "error",
            "error": "GDB command timed out after 10 seconds.",
        }, indent=2))]

    output = (result.stdout + result.stderr).strip()

    gdb_output = {
        "status": "ok",
        "pid": pid,
        "command": command,
        "output": output,
        "gdb_return_code": result.returncode,
    }
    return [TextContent(type="text", text=json.dumps(gdb_output, indent=2))]


async def main():
    async with stdio_server() as (read, write):
        await app.run(read, write, app.create_initialization_options())


if __name__ == "__main__":
    import asyncio
    asyncio.run(main())
