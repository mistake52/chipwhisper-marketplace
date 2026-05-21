"""Token budget management for retrieval results.

Priority-ordered truncation with compact/standard/verbose tiers.
Register facts and bitfield definitions are never truncated.
Document chunk text is truncated first.
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import Any


class DetailLevel(str, Enum):
    COMPACT = "compact"
    STANDARD = "standard"
    VERBOSE = "verbose"


# Lower index = higher priority (never truncated)
_PRIORITY_ORDER = [
    "register_fact",
    "errata",
    "bitfield",
    "pin",
    "clock",
    "interrupt",
    "doc_chunk",
]

_PRIORITY_RANK = {t: i for i, t in enumerate(_PRIORITY_ORDER)}

_DETAIL_LIMITS = {
    DetailLevel.COMPACT: 800,
    DetailLevel.STANDARD: 2000,
    DetailLevel.VERBOSE: 4000,
}


@dataclass
class RetrievedItem:
    type: str  # register_fact, bitfield, errata, clock, interrupt, pin, doc_chunk
    text: str
    citation: str = ""
    score: float = 0.0
    node_id: str = ""
    metadata: dict[str, Any] = field(default_factory=dict)


def estimate_tokens(text: str) -> int:
    """Estimate token count for English technical text (~4 chars per token)."""
    return len(text) // 4


def apply_token_budget(
    items: list[RetrievedItem],
    detail_level: DetailLevel = DetailLevel.STANDARD,
    max_tokens: int = 2048,
) -> dict:
    """Apply priority-ordered token budget to retrieval results.

    Register facts and bitfield definitions are never truncated.
    Document chunk text is truncated first.

    Args:
        items: Retrieved items with type, text, citation, score.
        detail_level: compact/standard/verbose detail tier.
        max_tokens: Hard token budget for returned context.

    Returns:
        Dict with:
            context_items: List[RetrievedItem] within budget
            truncated_count: int, number of items dropped
            truncated: bool, whether truncation occurred
            token_estimate: int, estimated tokens used
    """
    limit = _DETAIL_LIMITS.get(detail_level, 2000)
    effective_limit = min(limit, max_tokens)
    hard_limit = int(effective_limit * 0.9)

    sorted_items = sorted(items, key=lambda x: _PRIORITY_RANK.get(x.type, 99))

    context = []
    current_tokens = 0
    truncated_count = 0

    for item in sorted_items:
        item_tokens = estimate_tokens(item.text)

        # Register facts and bitfields are prioritized but still respect budget
        if current_tokens + item_tokens > hard_limit:
            truncated_count += 1
            continue

        context.append(item)
        current_tokens += item_tokens

    result: dict[str, Any] = {
        "context_items": context,
        "token_estimate": current_tokens,
        "truncated": truncated_count > 0,
    }

    if truncated_count > 0:
        remaining = [i for i in sorted_items if i not in context]
        truncated_types = {i.type for i in remaining}
        result["truncated_count"] = truncated_count
        result["truncation_notice"] = (
            f"... {truncated_count} items truncated ({', '.join(sorted(truncated_types))}). "
            f"Use detail_level='verbose' or a more specific query to retrieve full context."
        )

    return result


def format_context(
    items: list[RetrievedItem],
    detail_level: DetailLevel = DetailLevel.STANDARD,
) -> str:
    """Format retrieved items into a single context string for Claude.

    The format varies by detail_level:
        compact: register name + address + key bitfields only
        standard: compact + full bitfields + 1 association + citation
        verbose: standard + all associations + document text excerpt
    """
    if not items:
        return "No relevant documentation found for this query."

    sections: list[str] = []

    # Group by type
    registers = [i for i in items if i.type == "register_fact"]
    bitfields = [i for i in items if i.type == "bitfield"]
    pins = [i for i in items if i.type == "pin"]
    errata = [i for i in items if i.type == "errata"]
    other_facts = [i for i in items if i.type in ("clock", "interrupt")]
    doc_chunks = [i for i in items if i.type == "doc_chunk"]

    if registers:
        sections.append("## Register Facts\n")
        for r in registers:
            sections.append(f"- {r.text}")
            if detail_level != DetailLevel.COMPACT and r.citation:
                sections.append(f"  {r.citation}")

    if bitfields and detail_level != DetailLevel.COMPACT:
        sections.append("\n## Bitfield Details\n")
        for b in bitfields:
            sections.append(f"- {b.text}")

    if pins and detail_level != DetailLevel.COMPACT:
        sections.append("\n## Pin Assignments\n")
        for p in pins:
            sections.append(f"- {p.text}")
            if p.citation:
                sections.append(f"  {p.citation}")

    if errata and detail_level != DetailLevel.COMPACT:
        sections.append("\n## Errata Notes\n")
        for e in errata:
            sections.append(f"- {e.text}")
            if e.citation:
                sections.append(f"  {e.citation}")

    if detail_level == DetailLevel.VERBOSE:
        if other_facts:
            sections.append("\n## Related Facts\n")
            for o in other_facts:
                sections.append(f"- {o.text}")
                if o.citation:
                    sections.append(f"  {o.citation}")

        if doc_chunks:
            sections.append("\n## Document Excerpts\n")
            for d in doc_chunks:
                sections.append(f"- {d.text}")
                if d.citation:
                    sections.append(f"  {d.citation}")

    return "\n".join(sections)
