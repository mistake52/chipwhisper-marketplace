"""
Graph Builder for STM32F1 knowledge graph.

Takes SVDParser.to_graph_format() output, builds a NetworkX DiGraph,
adds Chip/Clock/Pin/Errata nodes and all edge types, persists to JSON.
"""

import json
import time
from pathlib import Path
from typing import Optional

import networkx as nx

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

CLOCK_ENABLE_MAP = {
    "GPIOA":  ("APB2", "RCC_APB2ENR", 2,  "0x40021018"),
    "GPIOB":  ("APB2", "RCC_APB2ENR", 3,  "0x40021018"),
    "GPIOC":  ("APB2", "RCC_APB2ENR", 4,  "0x40021018"),
    "GPIOD":  ("APB2", "RCC_APB2ENR", 5,  "0x40021018"),
    "GPIOE":  ("APB2", "RCC_APB2ENR", 6,  "0x40021018"),
    "GPIOF":  ("APB2", "RCC_APB2ENR", 7,  "0x40021018"),
    "GPIOG":  ("APB2", "RCC_APB2ENR", 8,  "0x40021018"),
    "ADC1":   ("APB2", "RCC_APB2ENR", 9,  "0x40021018"),
    "SPI1":   ("APB2", "RCC_APB2ENR", 12, "0x40021018"),
    "USART1": ("APB2", "RCC_APB2ENR", 14, "0x40021018"),
    "TIM2":   ("APB1", "RCC_APB1ENR", 0,  "0x4002101C"),
    "USART2": ("APB1", "RCC_APB1ENR", 17, "0x4002101C"),
    "I2C1":   ("APB1", "RCC_APB1ENR", 21, "0x4002101C"),
    "DMA1":   ("AHB",  "RCC_AHBENR",  0,  "0x40021014"),
    "FLASH":  ("AHB",  "RCC_AHBENR",  4,  "0x40021014"),
}

CHIP_SPEC = {
    "id": "STM32F103C8",
    "type": "Chip",
    "name": "STM32F103C8",
    "series": "STM32F1",
    "flash_kb": 64,
    "ram_kb": 20,
    "package": "LQFP48",
    "max_mhz": 72,
    "revision": "Rev X",
}


class GraphBuilder:
    """Build a NetworkX knowledge graph from SVD parsed data."""

    def __init__(self, graph_data: dict):
        self._data = graph_data
        self._g: Optional[nx.DiGraph] = None

    def build(self) -> nx.DiGraph:
        g = nx.DiGraph()

        for node in self._data["nodes"]:
            node_id = node["id"]
            g.add_node(node_id, **{k: v for k, v in node.items() if k != "id"})

        for edge in self._data["edges"]:
            g.add_edge(edge["source"], edge["target"], **{
                k: v for k, v in edge.items() if k not in ("source", "target")
            })

        self._add_chip_node(g)
        self._add_clock_nodes(g)
        self._add_requires_clock_edges(g)
        self._add_pin_nodes(g)
        self._add_located_at_pin_edges(g)
        self._add_errata_nodes(g)
        self._add_affected_by_errata_edges(g)
        self._add_relation_edges(g)

        self._g = g
        return g

    def _add_chip_node(self, g: nx.DiGraph) -> None:
        g.add_node(CHIP_SPEC["id"], **{k: v for k, v in CHIP_SPEC.items() if k != "id"})

    def _add_clock_nodes(self, g: nx.DiGraph) -> None:
        for periph_name, (domain, reg, bit, addr) in CLOCK_ENABLE_MAP.items():
            pid = f"F103_{periph_name}"
            clock_id = f"{pid}_CLKEN"
            g.add_node(clock_id,
                id=clock_id,
                type="Clock",
                name=f"{periph_name}CLK_EN",
                domain=domain,
                enable_reg=reg,
                enable_bit=bit,
                enable_reg_addr=addr,
                peripheral_id=pid,
            )

    def _add_requires_clock_edges(self, g: nx.DiGraph) -> None:
        clock_nodes = [n for n, d in g.nodes(data=True) if d.get("type") == "Clock"]
        for clock_id in clock_nodes:
            clock_data = g.nodes[clock_id]
            pid = clock_data["peripheral_id"]
            if pid in g:
                g.add_edge(pid, clock_id, type="REQUIRES_CLOCK", mandatory=True)

    def _add_pin_nodes(self, g: nx.DiGraph) -> None:
        pin_path = PROJECT_ROOT / "data" / "pin_mapping" / "stm32f103c8_lqfp48.json"
        if not pin_path.exists():
            return
        with open(pin_path) as f:
            data = json.load(f)

        for item in data.get("pins", []):
            if item.get("type") == "power" or item.get("type") == "reset" or item.get("type") == "boot":
                continue
            port = item.get("port", "")
            if not port:
                continue
            pin_id = f"F103_{item['pin']}"
            g.add_node(pin_id,
                id=pin_id,
                type="Pin",
                name=item["pin"],
                port=port,
                num=item.get("num", 0),
                af_map={"af_default": item.get("af_default", {}),
                         "af_remap": item.get("af_remap", {})},
                peripheral_id=f"F103_{port}",
            )

    def _add_located_at_pin_edges(self, g: nx.DiGraph) -> None:
        pin_path = PROJECT_ROOT / "data" / "pin_mapping" / "stm32f103c8_lqfp48.json"
        if not pin_path.exists():
            return
        with open(pin_path) as f:
            data = json.load(f)

        pin_nodes = {d.get("name"): n for n, d in g.nodes(data=True) if d.get("type") == "Pin"}
        for item in data.get("pins", []):
            pin_name = item.get("pin", "")
            pin_id = pin_nodes.get(pin_name)
            if not pin_id:
                continue
            all_functions = {}
            all_functions.update(item.get("af_default", {}))
            all_functions.update(item.get("af_remap", {}))
            for func_name, mode in all_functions.items():
                periph_name = func_name.split("_")[0]  # "USART1_TX" → "USART1"
                # Map to whitelisted peripheral names
                for prefix in ("USART1", "USART2", "SPI1", "SPI2", "SPI3",
                               "I2C1", "I2C2", "TIM1", "TIM2", "TIM3", "TIM4",
                               "ADC1", "DMA1"):
                    if func_name.startswith(prefix):
                        pid = f"F103_{prefix}"
                        if pid in g:
                            g.add_edge(pid, pin_id, type="LOCATED_AT_PIN",
                                      function=func_name, mode=mode)
                        break

    def _add_errata_nodes(self, g: nx.DiGraph) -> None:
        errata_path = PROJECT_ROOT / "data" / "errata" / "es0005_items.json"
        if not errata_path.exists():
            return
        with open(errata_path) as f:
            data = json.load(f)

        for item in data.get("items", []):
            eid = item["id"]
            g.add_node(eid,
                id=eid,
                type="Errata",
                name=eid,
                title=item["title"],
                affected_revisions=item.get("affected_revisions", []),
                workaround=item.get("workaround", ""),
                peripheral_id=f"F103_{item['peripheral']}",
            )

    def _add_affected_by_errata_edges(self, g: nx.DiGraph) -> None:
        errata_path = PROJECT_ROOT / "data" / "errata" / "es0005_items.json"
        if not errata_path.exists():
            return
        with open(errata_path) as f:
            data = json.load(f)

        for item in data.get("items", []):
            pid = f"F103_{item['peripheral']}"
            eid = item["id"]
            if pid in g and eid in g:
                g.add_edge(pid, eid, type="AFFECTED_BY_ERRATA",
                          severity=item.get("severity", "unknown"))

    def _add_relation_edges(self, g: nx.DiGraph) -> None:
        rel_path = PROJECT_ROOT / "data" / "graph" / "register_relations.json"
        if not rel_path.exists():
            return
        with open(rel_path) as f:
            data = json.load(f)

        for entry in data.get("clears_by", []):
            src, tgt = entry["source"], entry["target"]
            if src in g and tgt in g:
                g.add_edge(src, tgt, type="CLEARS_BY",
                          operation=entry.get("operation", ""),
                          note=entry.get("note", ""))

        for entry in data.get("depends_on", []):
            src, tgt = entry["source"], entry["target"]
            if src in g and tgt in g:
                g.add_edge(src, tgt, type="DEPENDS_ON",
                          order=entry.get("order", "before"),
                          note=entry.get("note", ""))

    @property
    def graph(self) -> nx.DiGraph:
        if self._g is None:
            return self.build()
        return self._g

    def save(self, filepath: str | Path) -> None:
        g = self.graph
        data = nx.node_link_data(g, edges="links")
        with open(filepath, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)

    @staticmethod
    def load(filepath: str | Path) -> nx.DiGraph:
        with open(filepath, encoding="utf-8") as f:
            data = json.load(f)
        return nx.node_link_graph(data, edges="links")

    def audit(self) -> dict:
        g = self.graph
        node_types = {}
        edge_types = {}
        for _, d in g.nodes(data=True):
            t = d.get("type", "Unknown")
            node_types[t] = node_types.get(t, 0) + 1
        for _, _, d in g.edges(data=True):
            t = d.get("type", "Unknown")
            edge_types[t] = edge_types.get(t, 0) + 1

        return {
            "node_count": g.number_of_nodes(),
            "edge_count": g.number_of_edges(),
            "node_types": dict(sorted(node_types.items())),
            "edge_types": dict(sorted(edge_types.items())),
        }

    @staticmethod
    def run_audit_report(filepath: str | Path) -> dict:
        t0 = time.time()
        g = GraphBuilder.load(filepath)
        load_ms = (time.time() - t0) * 1000

        builder = GraphBuilder.__new__(GraphBuilder)
        builder._g = g
        builder._data = {}
        audit = builder.audit()

        file_size = Path(filepath).stat().st_size
        audit["json_size_mb"] = round(file_size / (1024 * 1024), 2)
        audit["load_time_ms"] = round(load_ms, 2)
        audit["under_20k_nodes"] = audit["node_count"] < 20000
        audit["under_10mb"] = file_size < 10 * 1024 * 1024
        audit["under_1s_load"] = load_ms < 1000
        return audit
