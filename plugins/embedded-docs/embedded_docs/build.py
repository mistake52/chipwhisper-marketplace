#!/usr/bin/env python3
"""CLI and MCP tool handlers for building STM32 knowledge bases.

All functions accept a chip_model parameter (e.g. "STM32F103C8") and derive
paths, URLs, and config from data/chips.json via chip_registry.

Subcommands:
    download    Download PDFs from ST.com
    parse       Parse PDFs into structured Markdown
    build-graph Build knowledge graph from SVD + parsed markdown
    index       Embed chunks and upload to Qdrant
    all         Full pipeline: download -> parse -> build-graph -> index
"""

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path


def get_plugin_root() -> Path:
    env_root = os.environ.get("CLAUDE_PLUGIN_ROOT")
    if env_root:
        return Path(env_root)
    return Path(__file__).resolve().parent.parent


PLUGIN_ROOT = get_plugin_root()
DATA_DIR = PLUGIN_ROOT / "data"


def _get_config(chip_model: str) -> dict:
    from embedded_docs.chip_registry import get_chip_config
    return get_chip_config(chip_model, data_dir=DATA_DIR)


def _resolve_paths(chip_model: str):
    """Resolve all data paths for a given chip model."""
    cfg = _get_config(chip_model)
    rm = cfg.get("reference_manual", {})
    doc_id = rm.get("doc_id", "en.CD00171190")

    return {
        "svd": DATA_DIR / "svd" / cfg["svd"],
        "rm_md": DATA_DIR / "parsed" / doc_id / f"{doc_id}.md",
        "rm_sections": DATA_DIR / "parsed" / doc_id / f"{doc_id}_sections.json",
        "graph": DATA_DIR / "graph" / cfg["graph"],
        "chunks": DATA_DIR / "graph" / cfg.get("chunks", "chunks_rm0008.json"),
        "report": DATA_DIR / "graph" / "association_report.md",
        "collection": cfg.get("qdrant_collection", "stm32f1_docs"),
    }


def download_docs(chip_model: str = "STM32F103C8", force: bool = False) -> dict:
    """Download reference PDFs from ST.com for the given chip."""
    cfg = _get_config(chip_model)
    raw_dir = DATA_DIR / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)

    urls = {}
    rm = cfg.get("reference_manual", {})
    if rm.get("download_url"):
        urls[f"{rm['doc_id']}.pdf"] = rm["download_url"]
    ds = cfg.get("datasheet", {})
    if ds.get("download_url"):
        filename = os.path.basename(ds["download_url"])
        urls[filename] = ds["download_url"]
    err = cfg.get("errata", {})
    if err.get("download_url"):
        filename = err.get("file", os.path.basename(err["download_url"]))
        if not filename.endswith(".pdf"):
            filename = f"{filename}.pdf"
        urls[filename] = err["download_url"]

    results = {}
    for filename, url in urls.items():
        dest = raw_dir / filename
        if dest.exists() and not force:
            results[filename] = "skipped (exists)"
            continue
        try:
            subprocess.run(
                ["wget", "-q", "--show-progress",
                 "--user-agent", "Mozilla/5.0",
                 "-O", str(dest), url],
                check=True, timeout=120,
            )
            results[filename] = f"downloaded ({dest.stat().st_size:,} bytes)"
        except subprocess.CalledProcessError as e:
            results[filename] = f"WARNING: download failed (exit {e.returncode})"
        except Exception as e:
            results[filename] = f"WARNING: {e}"

    return {"ok": True, "status": "download complete", "files": results}


def parse_pdfs(chip_model: str = "STM32F103C8", force: bool = False) -> dict:
    """Parse all PDFs in data/raw/ into structured Markdown using pymupdf."""
    import fitz

    raw_dir = DATA_DIR / "raw"
    pdfs = sorted(raw_dir.glob("*.pdf"))
    if not pdfs:
        return {"ok": True, "status": "no PDFs found", "parsed": []}

    parsed_dir = DATA_DIR / "parsed"
    parsed_dir.mkdir(parents=True, exist_ok=True)
    results = []

    for pdf_path in pdfs:
        pdf_name = pdf_path.stem
        out_subdir = parsed_dir / pdf_name
        md_path = out_subdir / f"{pdf_name}.md"
        sections_path = out_subdir / f"{pdf_name}_sections.json"

        if md_path.exists() and not force:
            results.append({"file": pdf_path.name, "status": "skipped (exists)"})
            continue

        out_subdir.mkdir(parents=True, exist_ok=True)
        try:
            doc = fitz.open(str(pdf_path))
            toc = doc.get_toc()
            sections = _toc_to_sections(toc)

            full_lines = []
            section_index = []
            for sec in sections:
                prefix = "#" * min(sec["level"], 6)
                full_lines.append(f"\n{prefix} {sec['title']}\n")
                start_p = sec["start_page"]
                end_p = sec["end_page"] if sec["end_page"] is not None else doc.page_count - 1
                page_texts = []
                for p in range(start_p, min(end_p + 1, doc.page_count)):
                    try:
                        page_texts.append(doc[p].get_text("text"))
                    except Exception:
                        page_texts.append(f"[Error extracting page {p}]")
                combined = "\n".join(page_texts)
                full_lines.append(combined)
                section_index.append({
                    "section": sec["title"], "level": sec["level"],
                    "pages": f"{start_p + 1}-{end_p + 1}",
                    "char_count": len(combined),
                })

            md_path.write_text("\n".join(full_lines), encoding="utf-8")
            sections_path.write_text(json.dumps(section_index, indent=2, ensure_ascii=False), encoding="utf-8")
            doc.close()
            results.append({
                "file": pdf_path.name, "pages": doc.page_count if hasattr(doc, 'page_count') else "?",
                "sections": len(sections), "status": "parsed",
            })
        except Exception as e:
            results.append({"file": pdf_path.name, "status": f"ERROR: {e}"})

    return {"ok": True, "status": "parse complete", "parsed": results}


def build_graph(chip_model: str = "STM32F103C8", force: bool = False) -> dict:
    """Build knowledge graph from SVD + parsed markdown for the given chip."""
    paths = _resolve_paths(chip_model)
    cfg = _get_config(chip_model)

    if not force and paths["graph"].exists():
        return {"ok": True, "status": "skipped (graph exists)"}

    if not paths["svd"].exists():
        return {"ok": False, "error": f"SVD file not found: {paths['svd']}"}
    if not paths["rm_md"].exists():
        return {"ok": False, "error": f"Markdown not found: {paths['rm_md']}. Run parse step first."}

    sys.path.insert(0, str(PLUGIN_ROOT))
    import networkx as nx
    from src.parser.svd_parser import SVDParser
    from src.graph.graph_builder import GraphBuilder

    steps = {}

    t0 = time.time()
    svd = SVDParser(str(paths["svd"]), chip_config=cfg)
    graph_data = svd.to_graph_format()
    steps["svd"] = f"{len(graph_data['nodes'])} nodes, {len(graph_data['edges'])} edges ({time.time() - t0:.1f}s)"

    t0 = time.time()
    builder = GraphBuilder(graph_data, chip_config=cfg)
    g = builder.build()
    steps["graph"] = f"{g.number_of_nodes()} nodes, {g.number_of_edges()} edges ({time.time() - t0:.1f}s)"

    t0 = time.time()
    audit = builder.audit()
    builder.save(str(paths["graph"]))
    steps["audit"] = f"saved {paths['graph'].stat().st_size / 1024:.0f} KB, types: {audit['node_types']}"

    t0 = time.time()
    from src.parser.doc_chunker import DocChunker
    chunker = DocChunker(str(paths["rm_md"]), str(paths["rm_sections"]), chip_config=cfg)
    chunks = chunker.chunk_all()
    chunk_nodes = chunker.to_graph_nodes(chunks)
    reg_defs = sum(1 for c in chunks if c.is_register_definition)
    steps["chunk"] = f"{len(chunk_nodes)} chunks, {reg_defs} register defs ({time.time() - t0:.1f}s)"

    paths["chunks"].parent.mkdir(parents=True, exist_ok=True)
    paths["chunks"].write_text(json.dumps(chunk_nodes, indent=2, ensure_ascii=False), encoding="utf-8")

    t0 = time.time()
    from src.parser.doc_associator import DocAssociator
    associator = DocAssociator(graph_data, chunk_nodes)
    updated_chunks, assoc_edges = associator.run()
    report = associator.get_report()
    steps["association"] = (
        f"L1: {report['l1_anchor_count']}, L2: {report['l2_association_count']}, "
        f"{len(assoc_edges)} edges ({time.time() - t0:.1f}s)"
    )

    t0 = time.time()
    for chunk in updated_chunks:
        cid = chunk["id"]
        props = {k: v for k, v in chunk.items() if k != "id"}
        if len(props.get("text", "")) > 500:
            props["text"] = props["text"][:500] + "..."
        g.add_node(cid, **props)
    for edge in assoc_edges:
        g.add_edge(edge["source"], edge["target"],
                   **{k: v for k, v in edge.items() if k not in ("source", "target")})

    data = nx.node_link_data(g, edges="links")
    paths["graph"].parent.mkdir(parents=True, exist_ok=True)
    paths["graph"].write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")
    steps["merge"] = f"{g.number_of_nodes()} nodes, {g.number_of_edges()} edges, {paths['graph'].stat().st_size / 1024:.0f} KB ({time.time() - t0:.1f}s)"

    report_md_lines = [
        "# Document Association Report", "",
        "## Summary",
        f"- L1 definition anchors: **{report['l1_anchor_count']}**",
        f"- L2 usage associations: **{report['l2_association_count']}**",
    ]
    paths["report"].parent.mkdir(parents=True, exist_ok=True)
    paths["report"].write_text("\n".join(report_md_lines), encoding="utf-8")

    return {"ok": True, "status": "graph built", "steps": steps}


def index_chunks(chip_model: str = "STM32F103C8", force: bool = False) -> dict:
    """Embed document chunks and upload to Qdrant for the given chip."""
    from qdrant_client import QdrantClient
    from qdrant_client.models import Distance, VectorParams, PointStruct
    from sentence_transformers import SentenceTransformer

    paths = _resolve_paths(chip_model)
    collection_name = paths["collection"]

    if not paths["chunks"].exists():
        return {"ok": False, "error": f"Chunks not found: {paths['chunks']}. Run build-graph first."}

    try:
        client = QdrantClient(host="localhost", port=6333)
        client.get_collections()
    except Exception as e:
        return {"ok": False, "error": f"Qdrant not reachable at localhost:6333: {e}"}

    chunks = json.loads(paths["chunks"].read_text(encoding="utf-8"))

    if not force and client.collection_exists(collection_name):
        info = client.get_collection(collection_name)
        if info.points_count and info.points_count > 0:
            return {"ok": True, "status": "skipped (collection exists with data)", "vectors": info.points_count}

    model = SentenceTransformer("all-MiniLM-L6-v2")

    if client.collection_exists(collection_name):
        client.delete_collection(collection_name)
    client.create_collection(
        collection_name=collection_name,
        vectors_config=VectorParams(size=384, distance=Distance.COSINE),
    )

    total = 0
    batch_size = 32
    t0 = time.time()
    for i in range(0, len(chunks), batch_size):
        batch = chunks[i:i + batch_size]
        texts = [c["text"][:2000] for c in batch]
        embeddings = model.encode(texts, show_progress_bar=False)
        points = []
        for chunk, vector in zip(batch, embeddings):
            payload = {k: v for k, v in chunk.items() if k != "text"}
            payload["text"] = chunk["text"][:3000]
            points.append(PointStruct(id=total + len(points), vector=vector.tolist(), payload=payload))
        client.upsert(collection_name=collection_name, points=points)
        total += len(points)

    info = client.get_collection(collection_name)
    return {"ok": True, "status": "indexed", "vectors": total,
            "time_s": round(time.time() - t0, 1), "collection": info.points_count}


def run_all(chip_model: str = "STM32F103C8", *,
            skip_download: bool = False, skip_index: bool = False,
            force: bool = False) -> list[dict]:
    """Run full pipeline and return list of step results."""
    results = []
    if not skip_download:
        results.append({"step": "download", **download_docs(chip_model, force=force)})
    results.append({"step": "parse", **parse_pdfs(chip_model, force=force)})
    results.append({"step": "build-graph", **build_graph(chip_model, force=force)})
    if not skip_index:
        results.append({"step": "index", **index_chunks(chip_model, force=force)})
    return results


def _toc_to_sections(toc: list) -> list[dict]:
    if not toc:
        return []
    sections = []
    for i, (level, title, page) in enumerate(toc):
        title = title.strip()
        start_page = page - 1
        end_page = None
        for j in range(i + 1, len(toc)):
            next_level, _, next_page = toc[j]
            if next_level <= level:
                end_page = next_page - 2
                break
        sections.append({"level": level, "title": title, "start_page": start_page, "end_page": end_page})
    return sections


# ---- CLI ----

def main():
    parser = argparse.ArgumentParser(description="Build STM32 knowledge base for embedded-docs")
    parser.add_argument("--chip", default="STM32F103C8", help="Chip model (default: STM32F103C8)")
    parser.add_argument("--force", action="store_true", help="Overwrite existing outputs")
    sub = parser.add_subparsers(dest="command")

    sub.add_parser("download", help="Download PDFs from ST.com")
    sub.add_parser("parse", help="Parse PDFs into Markdown (pymupdf)")
    sub.add_parser("build-graph", help="Build knowledge graph from SVD + parsed markdown")
    sub.add_parser("index", help="Embed chunks and upload to Qdrant")
    all_parser = sub.add_parser("all", help="Run full pipeline")
    all_parser.add_argument("--skip-download", action="store_true")
    all_parser.add_argument("--skip-index", action="store_true")

    args = parser.parse_args()

    if args.command == "download":
        result = download_docs(chip_model=args.chip, force=args.force)
    elif args.command == "parse":
        result = parse_pdfs(chip_model=args.chip, force=args.force)
    elif args.command == "build-graph":
        result = build_graph(chip_model=args.chip, force=args.force)
    elif args.command == "index":
        result = index_chunks(chip_model=args.chip, force=args.force)
    elif args.command == "all":
        results = run_all(chip_model=args.chip, skip_download=args.skip_download,
                          skip_index=args.skip_index, force=args.force)
        for r in results:
            print(json.dumps(r, indent=2, ensure_ascii=False))
        return
    else:
        parser.print_help()
        return

    print(json.dumps(result, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
