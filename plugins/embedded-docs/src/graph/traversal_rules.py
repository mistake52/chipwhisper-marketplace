"""Graph traversal rules for intent-based edge filtering.

Provides entity extraction from query text and BFS traversal with
intent-specific edge type allowlists to prevent depth explosion.
Accepts chip_config for multi-chip support.
"""

import re
from dataclasses import dataclass, field

import networkx as nx

TRAVERSAL_RULES: dict[str, list[str]] = {
    "register_lookup": ["HAS_REGISTER", "HAS_BITFIELD", "DESCRIBED_IN", "CLEARS_BY"],
    "config_guide": ["HAS_REGISTER", "DEPENDS_ON", "REQUIRES_CLOCK", "DESCRIBED_IN"],
    "debug_diagnosis": ["HAS_REGISTER", "HAS_BITFIELD", "AFFECTED_BY_ERRATA", "CLEARS_BY", "DESCRIBED_IN"],
    "pin_mapping": ["LOCATED_AT_PIN", "HAS_REGISTER", "DESCRIBED_IN"],
    "clock_tree": ["REQUIRES_CLOCK", "DEPENDS_ON", "HAS_REGISTER", "DESCRIBED_IN"],
}

_REGISTER_PATTERN = re.compile(r"\b([A-Z][A-Z0-9]{1,}(?:_[A-Z][A-Z0-9]+)+)\b")
_ADDR_PATTERN = re.compile(r"(0x[0-9A-Fa-f]{8})")


@dataclass
class TraversalResult:
    nodes: list[dict] = field(default_factory=list)
    start_nodes: list[str] = field(default_factory=list)
    edge_types_used: list[str] = field(default_factory=list)
    depth: int = 0


def _expand_clock_registers(graph: nx.DiGraph, node_ids: list[str]) -> list[str]:
    """For each peripheral node, add its clock enable register to the start set.

    Handles RCC_ prefix stripping: Clock.enable_reg="RCC_APB2ENR" but
    Register.name="APB2ENR" (SVD strips the peripheral prefix from names).
    Also adds the parent RCC peripheral so traversal can reach other
    RCC registers (CFGR, CR) through HAS_REGISTER edges.
    """
    graph_nodes = dict(graph.nodes(data=True))
    expanded = list(node_ids)

    for nid in node_ids:
        for _, tgt, edata in graph.out_edges(nid, data=True):
            if edata.get("type") == "REQUIRES_CLOCK":
                clock_data = graph_nodes.get(tgt, {})
                enable_reg = clock_data.get("enable_reg", "")
                if enable_reg:
                    enable_reg_upper = enable_reg.upper()
                    for rid, rdata in graph_nodes.items():
                        if rdata.get("type") == "Register":
                            reg_name = rdata.get("name", "").upper()
                            # Match both "RCC_APB2ENR" and "APB2ENR" forms
                            # (SVD strips the RCC_ prefix from register names)
                            if reg_name == enable_reg_upper or \
                               enable_reg_upper.endswith("_" + reg_name):
                                if rid not in expanded:
                                    expanded.append(rid)
                                # Also add parent peripheral so other RCC
                                # registers (CFGR, CR) are reachable via HAS_REGISTER
                                parent_pid = rdata.get("peripheral_id", "")
                                if parent_pid and parent_pid not in expanded:
                                    expanded.append(parent_pid)

    return expanded


_PORT_NORMALIZE = re.compile(r"\bport\s+([A-G])\b", re.IGNORECASE)


def extract_query_entities(query: str, graph: nx.DiGraph,
                          chip_config: dict | None = None) -> list[str]:
    """Extract peripheral names and register names from a query, resolve to graph node IDs.

    Args:
        query: Natural language query string.
        graph: NetworkX DiGraph loaded from knowledge graph JSON.
        chip_config: Optional chip config dict. Uses traversal_peripheral_names
                     and node_prefix. Falls back to F103_ if omitted.

    Returns a list of node IDs in the graph that match entities in the query.
    """
    cfg = chip_config or {}
    periph_names = cfg.get("traversal_peripheral_names",
        ["RCC", "GPIOA", "GPIOB", "GPIOC", "GPIOD", "GPIOE", "GPIOF", "GPIOG",
         "USART1", "USART2", "SPI1", "I2C1", "TIM2", "ADC1", "DMA1", "FLASH", "NVIC"])
    prefix = cfg.get("node_prefix", "F103")

    # Normalize "port X" -> "GPIOX" before matching
    query = _PORT_NORMALIZE.sub(r"GPIO\1", query)

    node_ids: list[str] = []
    query_upper = query.upper()
    graph_nodes = dict(graph.nodes(data=True))

    # Track whether GPIO was matched generically vs specific port
    gpio_matched_specific = False

    # Match peripheral names
    for pname in periph_names:
        if pname in query_upper:
            if pname.startswith("GPIO") and len(pname) == 5:
                gpio_matched_specific = True
            pid = f"{prefix}_{pname}"
            if pid in graph_nodes:
                node_ids.append(pid)

    # GPIO generic match: expand to all GPIO ports if no specific port named
    if "GPIO" in query_upper and not gpio_matched_specific:
        for port_letter in "ABCDEFGHI":
            pname = f"GPIO{port_letter}"
            pid = f"{prefix}_{pname}"
            if pid in graph_nodes and pid not in node_ids:
                node_ids.append(pid)

    # Match register names (UPPERCASE_WITH_UNDERSCORES)
    for m in _REGISTER_PATTERN.finditer(query):
        reg_name = m.group(1)
        reg_upper = reg_name.upper()
        matched = False
        for nid, ndata in graph_nodes.items():
            if ndata.get("type") == "Register":
                node_name = ndata.get("name", "").upper()
                # Try exact match: "USART1_CR1" vs "USART1_CR1"
                if node_name == reg_upper:
                    if nid not in node_ids:
                        node_ids.append(nid)
                    matched = True
                # Try stripping peripheral prefix: "USART1_CR1" -> "CR1"
                elif "_" in reg_upper and node_name == reg_upper.split("_", 1)[1]:
                    if nid not in node_ids:
                        node_ids.append(nid)
                    matched = True
        # Fallback: try variant suffixes for SVD-split registers
        # Strip peripheral prefix for variant matching too
        if not matched:
            suffix_base = reg_upper.split("_", 1)[1] if "_" in reg_upper else reg_upper
            for suffix in ("_INPUT", "_OUTPUT", "_CAPTURE", "_COMPARE"):
                variant = suffix_base + suffix
                for nid, ndata in graph_nodes.items():
                    if ndata.get("type") == "Register" and ndata.get("name", "").upper() == variant:
                        if nid not in node_ids:
                            node_ids.append(nid)

    # Match hex addresses
    for m in _ADDR_PATTERN.finditer(query):
        addr = m.group(1).lower()
        addr_matched = False
        for nid, ndata in graph_nodes.items():
            if ndata.get("type") == "Register" and ndata.get("offset", "").lower() == addr:
                if nid not in node_ids:
                    node_ids.append(nid)
                addr_matched = True
            if ndata.get("type") == "Peripheral" and ndata.get("base_addr", "").lower() == addr:
                if nid not in node_ids:
                    node_ids.append(nid)
                addr_matched = True
        # Fallback: decompose full address as base_addr + register_offset
        # e.g. 0x40021018 = RCC base 0x40021000 + APB2ENR offset 0x18
        if not addr_matched and addr.startswith("0x"):
            try:
                addr_int = int(addr, 16)
            except ValueError:
                continue
            for pid, pdata in graph_nodes.items():
                if pdata.get("type") != "Peripheral":
                    continue
                base_str = pdata.get("base_addr", "")
                if not base_str:
                    continue
                try:
                    base_int = int(base_str, 16)
                except ValueError:
                    continue
                if addr_int < base_int:
                    continue
                offset_int = addr_int - base_int
                offset_hex = f"0x{offset_int:x}"
                for rid, rdata in graph_nodes.items():
                    if rdata.get("type") == "Register" and rdata.get("offset", "").lower() == offset_hex:
                        if pid not in node_ids:
                            node_ids.append(pid)
                        if rid not in node_ids:
                            node_ids.append(rid)

    # Keyword-to-peripheral fallback: domain keywords that imply RCC
    if not node_ids:
        _KV_PERIPHERAL = {
            "SYSTEM CLOCK": "RCC",
            "HSE": "RCC",
            "HSI": "RCC",
            "PLL": "RCC",
            "CRYSTAL": "RCC",
            "OSCILLATOR": "RCC",
            "OSC_IN": "RCC",
            "GPIO PORT CLOCK": "RCC",
        }
        for kw, pname in _KV_PERIPHERAL.items():
            if kw in query_upper:
                pid = f"{prefix}_{pname}"
                if pid in graph_nodes and pid not in node_ids:
                    node_ids.append(pid)

    node_ids = _expand_clock_registers(graph, node_ids)
    return node_ids


def traverse_for_intent(
    graph: nx.DiGraph,
    start_nodes: list[str],
    intent_type: str,
    max_depth: int = 3,
) -> TraversalResult:
    """Traverse the graph from start nodes, filtering edges by intent type.

    Uses multi-source BFS with edge type allowlist. Collects visited nodes
    and organizes them by type for the retrieval orchestrator.

    Args:
        graph: NetworkX DiGraph loaded from f1_knowledge_graph.json.
        start_nodes: Node IDs to start traversal from.
        intent_type: One of the INTENT_TYPES keys.
        max_depth: Maximum BFS depth (default 3).

    Returns:
        TraversalResult with visited nodes, start nodes, edge types used, and depth.
    """
    allowed_edges = TRAVERSAL_RULES.get(intent_type, [])
    result = TraversalResult(start_nodes=list(start_nodes), edge_types_used=list(allowed_edges))

    if not start_nodes or not allowed_edges:
        return result

    visited: set[str] = set(start_nodes)

    for depth in range(max_depth):
        new_nodes: set[str] = set()
        for src in visited:
            for _, tgt, edata in graph.out_edges(src, data=True):
                etype = edata.get("type", "")
                if etype in allowed_edges and tgt not in visited:
                    new_nodes.add(tgt)
        for src in visited:
            for pred, _, edata in graph.in_edges(src, data=True):
                etype = edata.get("type", "")
                if etype in allowed_edges and pred not in visited:
                    new_nodes.add(pred)
        if not new_nodes:
            break
        visited |= new_nodes

    result.depth = depth + 1

    # Collect node data
    graph_nodes = dict(graph.nodes(data=True))
    for nid in visited:
        if nid in graph_nodes:
            result.nodes.append({"id": nid, **graph_nodes[nid]})

    return result
