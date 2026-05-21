"""Intent classifier for STM32F1 queries.

Four-layer router: L0 hint → L1/L2 weighted regex rules (<1ms)
→ L3 REIC exemplar retrieval (<1ms) → L4 MiniLM zero-shot (<50ms).
Deterministic rules never self-demote — score > 0 returns immediately.
Zero LLM dependency.
"""

import re
from dataclasses import dataclass
from typing import Optional

INTENT_TYPES = [
    "register_lookup",
    "config_guide",
    "debug_diagnosis",
    "pin_mapping",
    "clock_tree",
]

INTENT_LABELS = {
    "register_lookup": "register address offset bitfield reset value access size",
    "config_guide": "configure initialize setup enable disable peripheral mode baud rate",
    "debug_diagnosis": "not working stuck error bug failure interrupt not firing unexpected behavior",
    "pin_mapping": "GPIO pin alternate function AF port mapping remap output input",
    "clock_tree": "clock RCC APB AHB prescaler divider HSE HSI PLL frequency enable",
}


@dataclass
class Intent:
    intent_type: str
    confidence: float
    source: str  # "rule" | "exemplar" | "zeroshot"


# Priority for tie-breaking when two intents have equal weighted scores.
# register_lookup wins ties because register facts are the highest-value answers.
_INTENT_PRIORITY = {
    "register_lookup": 5,
    "debug_diagnosis": 4,
    "config_guide": 3,
    "clock_tree": 2,
    "pin_mapping": 1,
}

# Rules: (regex_pattern, intent_type, weight)
# Weight 3 = strong signal (hex addr, bit notation, alternate function, remap)
# Weight 2 = intent-specific keywords
# Weight 1 = common peripheral terms that often appear across intent types
_RULES: list[tuple[re.Pattern, str, int]] = [
    # Weight 3 — strong signals
    (re.compile(r"0x[0-9A-Fa-f]{8}"), "register_lookup", 3),
    (re.compile(r"\b[A-Z][A-Z0-9_]{2,}\.[A-Z]"), "register_lookup", 3),
    (re.compile(r"\bbit\s*\d+\b", re.IGNORECASE), "register_lookup", 3),
    (re.compile(r"(?i)\bremap\b"), "pin_mapping", 3),
    (re.compile(r"(?i)\b(setup|configure|initialize)\s+\w+\s+pin\b"), "config_guide", 3),

    # Weight 2 — intent-specific keywords
    (re.compile(r"(?i)\b(reset\s+value|offset|access\s+type)\b"), "register_lookup", 2),
    (re.compile(r"(?i)\b(register|registers)\b"), "register_lookup", 1),
    (re.compile(r"(?i)\b(not\s+(?:working|firing|clearing|setting|starting|completing|responding)|no\s+response|garbled|stuck|bug|fail|crash|hang|wrong|unexpected|never\s+(?:sets|set|clears|fires|completes)|garbage\s+data|flag\s+timeout|incorrect)"), "debug_diagnosis", 2),
    (re.compile(r"(?i)\b(configure|setup|initialize|init|how\s+to)\b"), "config_guide", 2),
    (re.compile(r"(?i)\balternate\s+function\b"), "pin_mapping", 2),
    (re.compile(r"(?i)\b(clock|rcc|hse|hsi|pll|hclk|pclk|oscillator)\b"), "clock_tree", 2),
    (re.compile(r"(?i)\b(?:enable|disable)\s+(?:\w+\s+)?clock\b"), "clock_tree", 2),

    # Weight 1 — common peripheral terms (frequently appear across intent types)
    (re.compile(r"(?i)\b(?:enable|disable)\b"), "config_guide", 1),
    (re.compile(r"(?i)\b(?:gpio|pin)\b"), "pin_mapping", 1),
    (re.compile(r"(?i)\bport\s+[a-g]\b"), "pin_mapping", 1),
    (re.compile(r"(?i)\b(?:apb|ahb|prescaler|divider)\b"), "clock_tree", 1),
]

_label_embeddings = None


def _load_zeroshot():
    global _label_embeddings
    if _label_embeddings is not None:
        return
    from src.retrieval.shared_model import get_model
    model = get_model()
    _label_embeddings = {
        intent: model.encode(desc)
        for intent, desc in INTENT_LABELS.items()
    }


def _rule_classify(query: str) -> Optional[Intent]:
    scores: dict[str, float] = {}
    for pattern, intent_type, weight in _RULES:
        count = len(pattern.findall(query))
        if count > 0:
            scores[intent_type] = scores.get(intent_type, 0) + count * weight

    if not scores:
        return None

    best_intent = max(scores, key=lambda k: (scores[k], _INTENT_PRIORITY.get(k, 0)))
    best_score = scores[best_intent]
    total_score = sum(scores.values())
    confidence = min(0.95, 0.5 + (best_score / total_score) * 0.45)
    return Intent(intent_type=best_intent, confidence=confidence, source="rule")


def _zeroshot_classify(query: str) -> Intent:
    _load_zeroshot()
    from numpy import dot
    from numpy.linalg import norm
    from src.retrieval.shared_model import get_model
    query_vec = get_model().encode(query)
    best_intent = None
    best_score = -1.0
    for intent, label_vec in _label_embeddings.items():
        score = float(dot(query_vec, label_vec) / (norm(query_vec) * norm(label_vec)))
        if score > best_score:
            best_score = score
            best_intent = intent
    confidence = min(0.9, max(0.3, best_score))
    return Intent(intent_type=best_intent, confidence=confidence, source="zeroshot")


_exemplar_index = None  # list[tuple[ndarray, str]]


def _load_exemplars():
    """Build exemplar index from test_set_v0.json (lazy, first-call only)."""
    global _exemplar_index
    if _exemplar_index is not None:
        return
    _load_zeroshot()  # ensure label embeddings are computed
    from src.retrieval.shared_model import get_model
    import json
    from pathlib import Path
    model = get_model()
    test_set = Path(__file__).parent.parent.parent / "tests" / "test_set_v0.json"
    with open(test_set) as f:
        examples = json.load(f)
    _exemplar_index = [
        (model.encode(ex["query"]), ex["intent"])
        for ex in examples
    ]


def _exemplar_classify(query: str) -> Optional[Intent]:
    """Retrieve top-3 similar labeled queries, majority-vote intent.

    Returns None if best similarity < 0.70, signaling that L4 zeroshot
    should take over.
    """
    _load_exemplars()
    from numpy import dot
    from numpy.linalg import norm
    from src.retrieval.shared_model import get_model
    query_vec = get_model().encode(query)

    scored = []
    for emb, intent in _exemplar_index:
        sim = float(dot(query_vec, emb) / (norm(query_vec) * norm(emb)))
        scored.append((sim, intent))
    scored.sort(reverse=True)

    top_3 = scored[:3]
    votes: dict[str, int] = {}
    for sim, intent in top_3:
        votes[intent] = votes.get(intent, 0) + 1

    best_intent = max(votes, key=votes.get)
    best_sim = top_3[0][0]

    if best_sim > 0.70:
        return Intent(intent_type=best_intent, confidence=best_sim, source="exemplar")
    return None


def classify_intent(query: str, hint: str = "auto") -> Intent:
    """Classify a natural language query into one of 6 intent types.

    Args:
        query: Natural language query string.
        hint: Optional intent hint. One of the INTENT_TYPES or "auto".

    Returns:
        Intent with type, confidence, and source.
    """
    if hint and hint != "auto" and hint in INTENT_TYPES:
        return Intent(intent_type=hint, confidence=0.95, source="rule")

    # L1/L2: deterministic rules — score > 0 returns immediately
    rule_result = _rule_classify(query)
    if rule_result is not None:
        rule_result.confidence = 0.90
        return rule_result

    # L3: REIC exemplar retrieval — top-3 majority vote
    exemplar_result = _exemplar_classify(query)
    if exemplar_result is not None:
        return exemplar_result

    # L4: true fallback — MiniLM zero-shot
    return _zeroshot_classify(query)


def preload_model():
    """Preload the zeroshot model to avoid cold-start latency on first query."""
    _load_zeroshot()


def reset_model_cache():
    """Clear the lazy-loaded zeroshot model and exemplar index (useful for testing)."""
    global _label_embeddings, _exemplar_index
    _label_embeddings = None
    _exemplar_index = None
