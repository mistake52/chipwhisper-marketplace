"""
Document Chunker for parsed RM0008 markdown.

Reads the parsed markdown and sections JSON, creates DocumentChunk nodes
at register-definition granularity with peripheral hints.
"""

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

# Matches "X.Y.Z Description register (REG_NAME)"
REGISTER_DEF_PATTERN = re.compile(
    r"^\d+\.\d+\.\d+\s+.+?\s+register.*?\((\w+)\)", re.IGNORECASE
)

# GPIOx / TIMx generic register pattern: "GPIOx_CRL" or "TIMx_CR1" etc.
GENERIC_REG_PATTERN = re.compile(r"(\w+)x_(\w+)")

# Chapter prefix -> peripheral name mapping (longest prefix wins)
CHAPTER_TO_PERIPHERAL = {
    "27.6": "USART1",
    "27":   "USART1",
    "7.3":  "RCC",
    "7":    "RCC",
    "8.3":  "RCC",
    "9.2":  "GPIO",
    "9.1":  "GPIO",
    "9":    "GPIO",
    "15.4": "TIM2",
    "15":   "TIM2",
    "25.5": "SPI1",
    "25":   "SPI1",
    "26.6": "I2C1",
    "26":   "I2C1",
    "11.12": "ADC1",
    "11":   "ADC1",
    "13.4": "DMA1",
    "13":   "DMA1",
    "3.3":  "FLASH",
    "3":    "FLASH",
    "10.1": "NVIC",
    "10":   "NVIC",
}

# Headers and footers to strip from text
STRIP_PATTERNS = [
    re.compile(r"^RM0008\s*$", re.MULTILINE),
    re.compile(r"^RM0008 Rev \d+\s*$", re.MULTILINE),
    re.compile(r"^Doc ID \d+ Rev \d+\s*$", re.MULTILINE),
    re.compile(r"^\d+/\d+\s*$", re.MULTILINE),
]


@dataclass
class DocumentChunkData:
    chunk_id: str
    source: str
    revision: str
    page_start: int
    page_end: int
    section_number: str
    section_title: str
    text: str
    char_count: int
    peripheral_hint: str | None
    is_register_definition: bool
    register_names: list[str] = field(default_factory=list)


class DocChunker:
    def __init__(self, md_path: str | Path, sections_path: str | Path,
                 source: str = "RM0008", revision: str = "Rev 21"):
        self.md_path = Path(md_path)
        self.sections_path = Path(sections_path)
        self.source = source
        self.revision = revision
        self._md_text: str | None = None
        self._sections: list[dict] | None = None
        self._header_map: dict[str, dict] | None = None

    @property
    def md_text(self) -> str:
        if self._md_text is None:
            self._md_text = self.md_path.read_text(encoding="utf-8")
        return self._md_text

    @property
    def sections(self) -> list[dict]:
        if self._sections is None:
            self._sections = json.loads(self.sections_path.read_text(encoding="utf-8"))
        return self._sections

    def _parse_headers(self) -> dict[str, dict]:
        """Build {section_title: {line_no, level, end_line}} mapping from markdown."""
        header_map = {}
        lines = self.md_text.split("\n")

        header_positions = []
        for i, line in enumerate(lines):
            m = re.match(r"^(#{1,6})\s+(.+)", line)
            if m:
                level = len(m.group(1))
                title = m.group(2).strip()
                header_positions.append((i, level, title))

        for idx, (line_no, level, title) in enumerate(header_positions):
            if idx + 1 < len(header_positions):
                end_line = header_positions[idx + 1][0]
            else:
                end_line = len(lines)
            header_map[title] = {
                "line_no": line_no,
                "level": level,
                "end_line": end_line,
            }

        self._header_map = header_map
        return header_map

    def _clean_text(self, text: str) -> str:
        for pat in STRIP_PATTERNS:
            text = pat.sub("", text)
        text = re.sub(r"\n{3,}", "\n\n", text)
        return text.strip()

    def _extract_section_number(self, title: str) -> str:
        m = re.match(r"^(\d+\.\d+(?:\.\d+)?)", title)
        return m.group(1) if m else ""

    def _detect_peripheral(self, section_num: str) -> str | None:
        if not section_num:
            return None
        best = None
        best_len = 0
        for prefix, periph in CHAPTER_TO_PERIPHERAL.items():
            if section_num.startswith(prefix) and len(prefix) > best_len:
                best = periph
                best_len = len(prefix)
        return best

    def _detect_register_names(self, title: str) -> list[str]:
        m = REGISTER_DEF_PATTERN.match(title)
        if not m:
            return []
        return [m.group(1)]

    def _parse_page_range(self, pages_str: str) -> tuple[int, int]:
        parts = pages_str.split("-")
        nums = [int(p) for p in parts if p.lstrip("-").isdigit()]
        if not nums:
            return 1, 1
        return min(nums), max(nums)

    def _extract_by_pages(self, start_page: int, end_page: int) -> str:
        """Fallback: extract text by locating page markers in the markdown."""
        lines = self.md_text.split("\n")
        page_pat = re.compile(r"^(\d+)/\d+\s*$")

        in_range = False
        text_lines = []
        for line in lines:
            m = page_pat.match(line.strip())
            if m:
                page = int(m.group(1))
                if start_page <= page <= end_page:
                    in_range = True
                else:
                    in_range = False
                continue
            if in_range:
                text_lines.append(line)

        return self._clean_text("\n".join(text_lines))

    def chunk_all(self) -> list[DocumentChunkData]:
        headers = self._parse_headers()
        lines = self.md_text.split("\n")
        chunks = []
        counter = 0

        for sec in self.sections:
            title = sec["section"]
            info = headers.get(title)

            if info is None:
                for h_title, h_info in headers.items():
                    if title in h_title or h_title in title:
                        info = h_info
                        break
                if info is None:
                    continue

            line_no = info["line_no"]
            end_line = info["end_line"]
            text_lines = lines[line_no + 1:end_line]
            text = self._clean_text("\n".join(text_lines))

            start_page, end_page = self._parse_page_range(sec["pages"])

            # Page-based fallback for sections with misaligned header/content
            if not text.strip() or len(text) < 100:
                fallback = self._extract_by_pages(start_page, end_page)
                if fallback and len(fallback) > len(text):
                    text = fallback

            if not text.strip():
                continue

            start_page, end_page = self._parse_page_range(sec["pages"])
            section_num = self._extract_section_number(title)
            peripheral_hint = self._detect_peripheral(section_num)
            register_names = self._detect_register_names(title)
            is_reg_def = len(register_names) > 0

            chunk_id = self._make_chunk_id(start_page, section_num, counter)

            chunks.append(DocumentChunkData(

                chunk_id=chunk_id,
                source=self.source,
                revision=self.revision,
                page_start=start_page,
                page_end=end_page,
                section_number=section_num,
                section_title=title,
                text=text,
                char_count=len(text),
                peripheral_hint=peripheral_hint,
                is_register_definition=is_reg_def,
                register_names=register_names,
            ))
            counter += 1

        return chunks

    def _make_chunk_id(self, page: int, section_num: str, index: int = 0) -> str:
        sanitized = section_num.replace(".", "_") if section_num else f"p{page}"
        return f"chunk_rm0008_{page}_{sanitized}_{index}"

    def to_graph_nodes(self, chunks: list[DocumentChunkData] = None) -> list[dict]:
        if chunks is None:
            chunks = self.chunk_all()
        nodes = []
        for c in chunks:
            nodes.append({
                "id": c.chunk_id,
                "type": "DocumentChunk",
                "name": c.section_title[:80],
                "source": c.source,
                "revision": c.revision,
                "page": c.page_start,
                "section": c.section_number,
                "title": c.section_title,
                "text": c.text,
                "chip_series": "STM32F1",
                "linked_nodes": [],
                "association_confidence": 0.0,
                "association_level": "L3",
                "peripheral_hint": c.peripheral_hint,
                "is_register_definition": c.is_register_definition,
                "register_names": c.register_names,
            })
        return nodes
