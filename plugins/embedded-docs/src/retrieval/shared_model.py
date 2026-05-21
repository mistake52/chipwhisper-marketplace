"""Shared embedding model singleton for retrieval components.

Loads all-MiniLM-L6-v2 once and shares it between the intent classifier
(L4 zeroshot / L3 exemplar) and the retrieval orchestrator (vector search).
"""

from sentence_transformers import SentenceTransformer

_model = None


def get_model() -> SentenceTransformer:
    global _model
    if _model is None:
        _model = SentenceTransformer("all-MiniLM-L6-v2")
    return _model
