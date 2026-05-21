"""Retrieval orchestrator for STM32 technical queries.

End-to-end pipeline: intent classification -> graph traversal -> vector search
-> merge & rank -> token budget -> format response.
Accepts chip_config for multi-chip support.
"""

import time
from typing import Any, Optional

from qdrant_client import QdrantClient
from qdrant_client.models import FieldCondition, Filter, MatchValue

from src.graph.graph_builder import GraphBuilder
from src.graph.traversal_rules import extract_query_entities, traverse_for_intent
from src.retrieval.intent_classifier import Intent, classify_intent
from src.retrieval.token_budget import (
    DetailLevel,
    RetrievedItem,
    apply_token_budget,
    estimate_tokens,
    format_context,
)


class RetrievalOrchestrator:
    """End-to-end retrieval pipeline for STM32 documentation queries."""

    def __init__(self, graph_path: str = "data/graph/f1_knowledge_graph.json",
                 chip_config: dict | None = None):
        self.graph = GraphBuilder.load(graph_path)
        self.qdrant = QdrantClient(host="localhost", port=6333)
        self._cfg = chip_config or {}
        self._collection = self._cfg.get("qdrant_collection", "stm32f1_docs")
        self._series = self._cfg.get("series", "STM32F1")

    def warm_up(self):
        """Preload embedding and classification models to avoid cold-start latency."""
        from src.retrieval.intent_classifier import preload_model
        from src.retrieval.shared_model import get_model
        preload_model()
        get_model().encode("warmup")

    def _embed(self, text: str) -> list[float]:
        from src.retrieval.shared_model import get_model
        return get_model().encode(text).tolist()

    def query(
        self,
        query: str,
        chip_model: str = "STM32F103C8",
        doc_revision: str = "Rev 21",
        intent_hint: str = "auto",
        detail_level: str = "standard",
        max_tokens: int = 2048,
    ) -> dict[str, Any]:
        """Execute a full retrieval pipeline for the given query.

        Args:
            query: Natural language query string.
            chip_model: Target chip model (unused currently, single-chip).
            doc_revision: Reference manual revision filter.
            intent_hint: Optional intent override ("auto" for automatic).
            detail_level: compact/standard/verbose context verbosity.
            max_tokens: Hard token budget for returned context.

        Returns:
            Dict with answer_context, register_facts, citations, confidence, truncated.
        """
        t0 = time.perf_counter()

        # 1. Intent classification
        intent = classify_intent(query, hint=intent_hint)

        # 2. Entity extraction + graph traversal
        start_nodes = extract_query_entities(query, self.graph, chip_config=self._cfg)
        traversal = traverse_for_intent(self.graph, start_nodes, intent.intent_type)

        # 3. Vector search
        query_vec = self._embed(query)
        vector_results = self.qdrant.query_points(
            collection_name=self._collection,
            query=query_vec,
            limit=5,
            query_filter=Filter(
                must=[FieldCondition(key="chip_series", match=MatchValue(value=self._series))]
            ),
        ).points

        # 4. Merge graph + vector results
        merged_items = self._merge(traversal.nodes, vector_results, intent)

        # 5. Token budget
        detail = DetailLevel(detail_level)
        budget_result = apply_token_budget(merged_items, detail, max_tokens)

        # 6. Format response
        context = format_context(budget_result["context_items"], detail)
        register_facts = [
            i for i in budget_result["context_items"] if i.type == "register_fact"
        ]
        citations = list({i.citation for i in budget_result["context_items"] if i.citation})

        elapsed = time.perf_counter() - t0

        return {
            "answer_context": context,
            "register_facts": [
                {"name": r.node_id, "text": r.text, "citation": r.citation}
                for r in register_facts
            ],
            "citations": citations,
            "confidence": self._assess_confidence(traversal.nodes, vector_results),
            "truncated": budget_result["truncated"],
            "intent": intent.intent_type,
            "intent_confidence": round(intent.confidence, 3),
            "latency_ms": round(elapsed * 1000),
        }

    def _merge(
        self,
        graph_nodes: list[dict],
        vector_points: list[Any],
        intent: Intent,
    ) -> list[RetrievedItem]:
        """Merge graph traversal and vector search results.

        Graph results (precise facts) get priority. Vector results that overlap
        with graph nodes are boosted. Pure L3 vector results are appended as
        supplementary context.
        """
        items: list[RetrievedItem] = []
        seen_ids: set[str] = set()

        # Graph nodes first (high confidence structural facts)
        for nd in graph_nodes:
            ntype = nd.get("type", "")
            nid = nd.get("id", "")
            if nid in seen_ids:
                continue
            seen_ids.add(nid)

            item_type = _map_node_type(ntype)
            text = _format_graph_node(nd)
            citation = _make_citation(nd)
            items.append(RetrievedItem(
                type=item_type,
                text=text,
                citation=citation,
                score=1.0,
                node_id=nid,
                metadata=nd,
            ))

        # Vector results
        for vp in vector_points:
            payload = getattr(vp, "payload", {}) or {}
            vid = payload.get("id", str(vp.id))
            score = getattr(vp, "score", 0.0)

            if vid in seen_ids:
                # Boost graph item score if vector also finds it
                for item in items:
                    if item.node_id == vid:
                        item.score = max(item.score, 0.5 + score * 0.5)
                continue

            seen_ids.add(vid)
            items.append(RetrievedItem(
                type="doc_chunk",
                text=payload.get("text", ""),
                citation=_make_citation(payload),
                score=score,
                node_id=vid,
                metadata=payload,
            ))

        return items

    def _assess_confidence(
        self, graph_nodes: list[dict], vector_points: list[Any]
    ) -> str:
        """Assess overall result confidence (graph-centric).

        high: Register or BitField nodes hit (precise structural facts).
        medium: Peripheral/Clock/Interrupt nodes hit, or decent vector match.
        low: vector-only results with no graph anchor.
        """
        has_precise = any(
            nd.get("type") in ("Register", "BitField")
            for nd in graph_nodes
        )
        has_any = bool(graph_nodes)

        v_scores = [getattr(vp, "score", 0.0) for vp in vector_points]
        max_v = max(v_scores) if v_scores else 0.0

        if has_precise:
            return "high"
        if has_any or max_v > 0.5:
            return "medium"
        return "low"


def _map_node_type(graph_type: str) -> str:
    mapping = {
        "Register": "register_fact",
        "BitField": "bitfield",
        "Errata": "errata",
        "Clock": "clock",
        "Interrupt": "interrupt",
        "Pin": "pin",
        "DocumentChunk": "doc_chunk",
        "Peripheral": "register_fact",
        "Chip": "register_fact",
    }
    return mapping.get(graph_type, "doc_chunk")


def _format_graph_node(nd: dict) -> str:
    ntype = nd.get("type", "")
    name = nd.get("name", nd.get("id", "?"))
    if ntype == "Register":
        addr = nd.get("offset", "?")
        size = nd.get("size", 32)
        access = nd.get("access", "?")
        reset = nd.get("reset_value", "?")
        return f"{name} (offset={addr}, size={size}-bit, access={access}, reset={reset})"
    if ntype == "BitField":
        start = nd.get("bit_start", 0)
        end = nd.get("bit_end", 0)
        desc = nd.get("description", "")
        rw = nd.get("rw", "")
        bits = f"bit {start}" if start == end else f"bits [{start}:{end}]"
        return f"{name}: {bits}, {rw} — {desc}"
    if ntype == "Peripheral":
        addr = nd.get("base_addr", "?")
        bus = nd.get("bus", "?")
        return f"{name} (base_addr={addr}, bus={bus})"
    if ntype == "Clock":
        domain = nd.get("domain", "?")
        en_reg = nd.get("enable_reg", "?")
        en_bit = nd.get("enable_bit", "?")
        return f"{name}: {domain} clock, enable via {en_reg} bit {en_bit}"
    if ntype == "Interrupt":
        irqn = nd.get("irqn", "?")
        return f"{name}: IRQn={irqn}"
    if ntype == "Pin":
        port = nd.get("port", "?")
        num = nd.get("num", "?")
        af = nd.get("af_map", {})
        parts = [f"{name}: {port}{num}"]
        for af_kind in ("af_default", "af_remap"):
            afs = af.get(af_kind, {})
            for func, mode in afs.items():
                parts.append(f"{func}={mode}")
        return ", ".join(parts)
    if ntype == "Errata":
        title = nd.get("title", "")
        revs = nd.get("affected_revisions", [])
        workaround = nd.get("workaround", "")
        rev_str = f" (affected: {', '.join(revs)})" if revs else ""
        wa_str = f" Workaround: {workaround}" if workaround else ""
        return f"{name}: {title}{rev_str}.{wa_str}"
    return f"{name}"


def _make_citation(nd: dict) -> str:
    source = nd.get("source", "")
    rev = nd.get("revision", "")
    section = str(nd.get("section", ""))
    page = nd.get("page", "")
    if source:
        return f"[{source} {rev}, {section}, P{page}]".replace("  ", " ")
    return ""
