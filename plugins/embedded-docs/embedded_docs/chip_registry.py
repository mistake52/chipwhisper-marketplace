"""Chip configuration registry — loads data/chips.json.

Provides a single source of truth for all chip-specific configuration:
SVD paths, graph files, Qdrant collections, clock maps, bus topologies,
peripheral whitelists, reference manual metadata, and more.

Usage:
    from embedded_docs.chip_registry import get_chip_config, list_chip_models

    config = get_chip_config("STM32F103C8")
    config["node_prefix"]  # "F103"
    config["svd"]          # "STM32F103xx.svd"
"""

import json
from pathlib import Path


_REGISTRY: dict | None = None
_REGISTRY_PATH: Path | None = None


def load_registry(data_dir: Path | None = None) -> dict:
    """Load the chip registry from data/chips.json. Cached after first call."""
    global _REGISTRY, _REGISTRY_PATH

    if _REGISTRY is not None and data_dir is None:
        return _REGISTRY

    if data_dir is not None:
        _REGISTRY_PATH = data_dir

    if _REGISTRY_PATH is None:
        raise RuntimeError("Registry not initialized. Call load_registry(data_dir) first.")

    path = _REGISTRY_PATH / "chips.json"
    _REGISTRY = json.loads(path.read_text(encoding="utf-8"))
    return _REGISTRY


def get_chip_config(chip_model: str, data_dir: Path | None = None) -> dict:
    """Get configuration for a specific chip model.

    Args:
        chip_model: e.g. "STM32F103C8", "STM32F407VG"
        data_dir: Path to the data/ directory. Only needed on first call or if
                  switching plugin roots.

    Returns:
        dict with keys: series, node_prefix, svd, graph, qdrant_collection,
        chip_spec, peripheral_whitelist, bus_map, clock_enable_map, etc.

    Raises:
        ValueError: if chip_model is not in the registry.
    """
    registry = load_registry(data_dir)
    if chip_model not in registry:
        raise ValueError(
            f"Unknown chip: {chip_model}. Known: {list(registry.keys())}"
        )
    return registry[chip_model]


def list_chip_models(data_dir: Path | None = None) -> list[str]:
    """List all chip models in the registry."""
    registry = load_registry(data_dir)
    return list(registry.keys())
