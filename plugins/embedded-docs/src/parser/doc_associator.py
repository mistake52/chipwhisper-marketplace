"""
Document Association Engine for STM32F1 knowledge graph.

Implements three-level confidence association (L1/L2/L3) between
DocumentChunk nodes and graph Register nodes, creating DESCRIBED_IN edges.

L1: Register definition section titles (confidence=1.0)
L2: Text mentioning register names within peripheral chapter (confidence=0.75)
L3: Cross-chapter mentions — no graph edges, vector-search only (confidence=0.3)
"""

import random
import re
from collections import defaultdict


class DocAssociator:
    def __init__(self, graph_data: dict, chunk_nodes: list[dict]):
        self._graph_data = graph_data
        self._chunk_nodes = chunk_nodes

        # Build lookup: peripheral_id -> {register_name -> register_node}
        self._reg_by_periph: dict[str, dict[str, dict]] = defaultdict(dict)
        # Build lookup: register_node_id -> register_node
        self._reg_by_id: dict[str, dict] = {}
        # Build peripheral hint -> list of peripheral IDs (for GPIO expansion)
        self._periph_hint_to_ids: dict[str, list[str]] = defaultdict(list)

        for node in graph_data["nodes"]:
            if node["type"] == "Register":
                pid = node.get("peripheral_id", "")
                self._reg_by_periph[pid][node["name"]] = node
                self._reg_by_id[node["id"]] = node
            elif node["type"] == "Peripheral":
                hint = self._to_hint(node["name"])
                if "id" in node:
                    self._periph_hint_to_ids[hint].append(node["id"])

        self._l1_edges: list[dict] = []
        self._l1_matches: list[dict] = []  # For report
        self._l2_edges: list[dict] = []

    @staticmethod
    def _to_hint(name: str) -> str:
        """Normalize a peripheral name to its hint form (e.g. GPIOA -> GPIO)."""
        upper = name.upper()
        if upper.startswith("GPIO"):
            return "GPIO"
        return upper

    def run(self) -> tuple[list[dict], list[dict]]:
        """Run full association. Returns (updated_chunk_nodes, new_edges)."""
        updated = []

        for chunk in self._chunk_nodes:
            chunk_copy = dict(chunk)
            edges_from_chunk = []

            if chunk.get("is_register_definition") and chunk.get("register_names"):
                l1_edges = self._match_l1(chunk)
                if l1_edges:
                    edges_from_chunk.extend(l1_edges)
                    chunk_copy["association_level"] = "L1"
                    chunk_copy["association_confidence"] = 1.0
                    target_ids = [e["target"] for e in l1_edges]  # chunk is target
                    # Actually DESCRIBED_IN edge: source=Register, target=DocumentChunk
                    linked = [e["source"] for e in l1_edges]
                    chunk_copy["linked_nodes"] = linked

            if not edges_from_chunk and chunk.get("peripheral_hint"):
                l2_edges = self._match_l2(chunk)
                if l2_edges:
                    edges_from_chunk.extend(l2_edges)
                    chunk_copy["association_level"] = "L2"
                    chunk_copy["association_confidence"] = 0.75
                    linked = [e["source"] for e in l2_edges]
                    chunk_copy["linked_nodes"] = linked

            if not edges_from_chunk:
                chunk_copy["association_level"] = "L3"
                chunk_copy["association_confidence"] = 0.3

            updated.append(chunk_copy)

        all_edges = self._l1_edges + self._l2_edges
        return updated, all_edges

    def _match_l1(self, chunk: dict) -> list[dict]:
        """Match L1 anchor: register definition section -> graph Register node.

        Returns list of DESCRIBED_IN edges (source=Register, target=DocumentChunk).
        """
        edges = []
        periph_hint = chunk.get("peripheral_hint")
        raw_names = chunk.get("register_names", [])

        for raw_name in raw_names:
            matched = self._resolve_register(raw_name, periph_hint)
            for reg_id in matched:
                edge = {
                    "source": reg_id,
                    "target": chunk["id"],
                    "type": "DESCRIBED_IN",
                    "confidence": 1.0,
                    "level": "L1",
                }
                edges.append(edge)
                self._l1_edges.append(edge)
                self._l1_matches.append({
                    "chunk_id": chunk["id"],
                    "chunk_title": chunk.get("section_title", ""),
                    "raw_register_name": raw_name,
                    "matched_register_id": reg_id,
                    "page": chunk.get("page"),
                })

        return edges

    def _resolve_register(self, raw_name: str, periph_hint: str | None) -> list[str]:
        """Resolve a raw register name from a section title to graph register IDs.

        Only searches within the hinted peripheral — never across all peripherals,
        to avoid false positives (e.g., CRC_DR matching USART1_DR).
        """
        if not periph_hint:
            return []

        pids = self._periph_hint_to_ids.get(periph_hint.upper(), [])
        if not pids:
            return []

        candidates = self._get_candidate_names(raw_name, periph_hint)

        matched_ids = []
        for pid in pids:
            regs = self._reg_by_periph.get(pid, {})
            for cand in candidates:
                if cand in regs:
                    matched_ids.append(regs[cand]["id"])

        return matched_ids

    def _get_candidate_names(self, raw_name: str, periph_hint: str | None) -> list[str]:
        """Generate candidate register names for matching against SVD names."""
        cands = [raw_name]

        # Try stripping peripheral prefix (e.g., USART_SR -> SR)
        if periph_hint:
            prefix = periph_hint.upper() + "_"
            if raw_name.upper().startswith(prefix):
                cands.append(raw_name[len(prefix):])

        # Try stripping RCC_ prefix specifically
        if raw_name.upper().startswith("RCC_"):
            cands.append(raw_name[4:])

        # GPIOx/TIMx pattern: strip the 'x' variant
        # e.g., GPIOx_CRL -> GPIO_CRL, and also strip GPIO_ -> CRL
        import re as _re
        m = _re.match(r"(\w+)x_(\w+)", raw_name)
        if m:
            cands.append(m.group(2))  # just the suffix: CRL
            cands.append(f"{m.group(1)}A_{m.group(2)}")  # concrete: GPIOA_CRL

        # Generic: strip everything before last underscore if more than one
        parts = raw_name.split("_")
        if len(parts) > 1:
            cands.append(parts[-1])  # SR from USART_SR
            cands.append("_".join(parts[1:]))  # APB2ENR from RCC_APB2ENR

        # Deduplicate while preserving order
        seen = set()
        result = []
        for c in cands:
            if c not in seen:
                seen.add(c)
                result.append(c)
        return result

    def _match_l2(self, chunk: dict) -> list[dict]:
        """Match L2: text within peripheral chapter mentions register names.

        Only associates if register names from the hinted peripheral appear in text.
        """
        edges = []
        periph_hint = chunk.get("peripheral_hint")
        if not periph_hint:
            return edges

        pids = self._periph_hint_to_ids.get(periph_hint.upper(), [])
        text = chunk.get("text", "")

        for pid in pids:
            regs = self._reg_by_periph.get(pid, {})
            for reg_name, reg_node in regs.items():
                if len(reg_name) < 2:
                    continue
                # Word-boundary match
                pattern = re.compile(r"\b" + re.escape(reg_name) + r"\b")
                if pattern.search(text):
                    edge = {
                        "source": reg_node["id"],
                        "target": chunk["id"],
                        "type": "DESCRIBED_IN",
                        "confidence": 0.75,
                        "level": "L2",
                    }
                    edges.append(edge)
                    self._l2_edges.append(edge)

        return edges

    def get_report(self) -> dict:
        """Generate association summary report."""
        if not self._l1_matches and not self._l2_edges:
            self.run()

        l2_sample_size = max(1, len(self._l2_edges) // 5)
        l2_sample = random.sample(self._l2_edges, min(l2_sample_size, len(self._l2_edges)))

        # Count L1 by peripheral
        l1_by_periph = defaultdict(int)
        for m in self._l1_matches:
            pid = m["matched_register_id"].rsplit("_", 1)[0] if "_" in m["matched_register_id"] else "unknown"
            l1_by_periph[pid] += 1

        return {
            "l1_anchor_count": len(self._l1_matches),
            "l2_association_count": len(self._l2_edges),
            "l1_anchors": self._l1_matches,
            "l1_by_peripheral": dict(l1_by_periph),
            "l2_sample_for_review": [
                {"source": e["source"], "target": e["target"], "confidence": e["confidence"]}
                for e in l2_sample
            ],
        }
