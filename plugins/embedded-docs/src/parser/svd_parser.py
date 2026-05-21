"""
SVD Parser — thin wrapper around cmsis-svd library.

Converts CMSIS-SVD parsed data into PRD-defined node/edge graph format
(Section 4 data model) with peripheral whitelist filtering.
Accepts chip_config dict for multi-chip support.
"""

from dataclasses import dataclass, field, asdict
from typing import Optional

from cmsis_svd.parser import SVDParser as CmsisSVDParser


@dataclass
class BitFieldNode:
    name: str
    description: str
    bit_offset: int
    bit_width: int
    bit_start: int
    bit_end: int
    access: str


@dataclass
class RegisterNode:
    name: str
    description: str
    address_offset: str
    size: int
    access: str
    reset_value: str
    fields: list = field(default_factory=list)
    derived_from: Optional[str] = None


@dataclass
class InterruptNode:
    name: str
    description: str
    irq_number: int


@dataclass
class PeripheralNode:
    name: str
    base_address: str
    description: str
    bus: str
    registers: list = field(default_factory=list)
    interrupts: list = field(default_factory=list)


@dataclass
class DeviceTree:
    device_name: str
    device_version: str = ""
    vendor: str = ""
    peripherals: list = field(default_factory=list)


def _get_bus(periph_name: str, bus_map: dict) -> str:
    """Determine bus domain from peripheral name using chip's bus map."""
    upper = periph_name.upper()
    # Try AHB first (it's the broadest — includes DMA, FLASH, RCC, FSMC, etc.)
    for p in bus_map.get("AHB", []):
        if upper == p or upper.startswith(p):
            return "AHB"
    for p in bus_map.get("APB1", []):
        if upper == p or upper.startswith(p):
            return "APB1"
    for p in bus_map.get("APB2", []):
        if upper == p:
            return "APB2"
    if upper.startswith("GPIO"):
        return "APB2"
    return "APB1"


def _is_whitelisted(name: str, whitelist: set, prefix_patterns: set) -> bool:
    """Check if peripheral name matches whitelist (case-insensitive)."""
    upper = name.upper()
    for pattern in prefix_patterns:
        if upper.startswith(pattern):
            return True
    return upper in whitelist


def _access_to_str(access) -> str:
    """Convert cmsis-svd SVDAccessType enum to string."""
    if access is None:
        return "RW"
    name = access.name if hasattr(access, 'name') else str(access)
    mapping = {
        "READ_ONLY": "R",
        "READ_WRITE": "RW",
        "WRITE_ONLY": "W",
        "READ_WRITEONCE": "RW",
        "WRITE_ONCE": "W",
    }
    return mapping.get(name, "RW")


class SVDParser:
    """Parse STM32 SVD file and produce PRD-compliant graph data."""

    def __init__(self, svd_path: str, chip_config: dict | None = None):
        self.svd_path = svd_path
        self._cfg = chip_config or {}
        self._cmsis_parser = CmsisSVDParser.for_xml_file(svd_path)
        self._device = self._cmsis_parser.get_device()
        self._parsed_peripherals: list[PeripheralNode] = []

    @property
    def device_name(self) -> str:
        return self._device.name

    @property
    def vendor(self) -> str:
        return self._device.vendor or "STMicroelectronics"

    def parse_all(self) -> DeviceTree:
        """Parse all whitelisted peripherals from the SVD."""
        device = DeviceTree(
            device_name=self._device.name,
            device_version=self._device.version or "",
            vendor=self.vendor,
        )

        whitelist = set(self._cfg.get("peripheral_whitelist", []))
        prefix_patterns = set(self._cfg.get("prefix_patterns", []))
        for cmsis_periph in self._device.peripherals:
            if not _is_whitelisted(cmsis_periph.name, whitelist, prefix_patterns):
                continue

            periph = self._parse_peripheral(cmsis_periph)
            self._parsed_peripherals.append(periph)
            device.peripherals.append(periph)

        return device

    def parse_peripheral(self, name: str) -> Optional[PeripheralNode]:
        """Parse a single peripheral by exact name."""
        for cmsis_periph in self._device.peripherals:
            if cmsis_periph.name.upper() == name.upper():
                return self._parse_peripheral(cmsis_periph)
        return None

    def _parse_peripheral(self, cmsis_periph) -> PeripheralNode:
        base_addr = f"0x{cmsis_periph.base_address:08X}"
        bus = _get_bus(cmsis_periph.name, self._cfg.get("bus_map", {}))

        registers = []
        for cmsis_reg in cmsis_periph.registers:
            reg = self._parse_register(cmsis_reg)
            registers.append(reg)

        interrupts = []
        for cmsis_irq in (cmsis_periph.interrupts or []):
            interrupts.append(InterruptNode(
                name=cmsis_irq.name,
                description=cmsis_irq.description or "",
                irq_number=cmsis_irq.value,
            ))

        return PeripheralNode(
            name=cmsis_periph.name,
            base_address=base_addr,
            description=(cmsis_periph.description or "").strip(),
            bus=bus,
            registers=registers,
            interrupts=interrupts,
        )

    def _parse_register(self, cmsis_reg) -> RegisterNode:
        fields = []
        for cmsis_field in cmsis_reg.fields:
            fields.append(BitFieldNode(
                name=cmsis_field.name,
                description=(cmsis_field.description or "").strip(),
                bit_offset=cmsis_field.bit_offset,
                bit_width=cmsis_field.bit_width,
                bit_start=cmsis_field.bit_offset,
                bit_end=cmsis_field.bit_offset + cmsis_field.bit_width - 1,
                access=_access_to_str(cmsis_field.access),
            ))

        return RegisterNode(
            name=cmsis_reg.name,
            description=(cmsis_reg.description or "").strip(),
            address_offset=f"0x{cmsis_reg.address_offset:02X}",
            size=cmsis_reg.size,
            access=_access_to_str(cmsis_reg.access),
            reset_value=f"0x{cmsis_reg.reset_value:08X}",
            fields=fields,
            derived_from=cmsis_reg.derived_from or None,
        )

    def to_graph_format(self) -> dict:
        """Convert parsed peripherals to PRD graph format (nodes + edges).

        Returns dict with 'nodes' and 'edges' lists matching the schema in
        data/graph/graph_schema_v0.json.
        """
        if not self._parsed_peripherals:
            self.parse_all()

        nodes = []
        edges = []

        prefix = self._cfg.get("node_prefix", "F103")
        chip_id = self._cfg.get("chip_spec", {}).get("id",
                   self._cfg.get("chip_spec", {}).get("name", "UNKNOWN"))

        for periph in self._parsed_peripherals:
            pid = f"{prefix}_{periph.name}"

            # Peripheral node
            nodes.append({
                "id": pid,
                "type": "Peripheral",
                "name": periph.name,
                "base_addr": periph.base_address,
                "bus": periph.bus,
                "version": "v1",
                "chip_id": chip_id,
            })

            # Register nodes + HAS_REGISTER edges
            for i, reg in enumerate(periph.registers):
                rid = f"{pid}_{reg.name}"
                nodes.append({
                    "id": rid,
                    "type": "Register",
                    "name": reg.name,
                    "fullname": reg.name,
                    "offset": reg.address_offset,
                    "reset_value": reg.reset_value,
                    "access": reg.access,
                    "size": reg.size,
                    "peripheral_id": pid,
                })
                edges.append({
                    "source": pid,
                    "target": rid,
                    "type": "HAS_REGISTER",
                    "order": i,
                })

                # BitField nodes + HAS_BITFIELD edges
                for field in reg.fields:
                    fid = f"{rid}_{field.name}"
                    nodes.append({
                        "id": fid,
                        "type": "BitField",
                        "name": field.name,
                        "bit_start": field.bit_start,
                        "bit_end": field.bit_end,
                        "rw": field.access,
                        "description": field.description,
                        "register_id": rid,
                    })
                    edges.append({
                        "source": rid,
                        "target": fid,
                        "type": "HAS_BITFIELD",
                        "bit_mask": f"0x{((1 << field.bit_width) - 1) << field.bit_offset:X}",
                    })

            # Interrupt nodes + MAPS_TO_INTERRUPT edges
            for irq in periph.interrupts:
                iid = f"{pid}_{irq.name}_IRQn"
                nodes.append({
                    "id": iid,
                    "type": "Interrupt",
                    "name": f"{irq.name}_IRQn",
                    "irqn": irq.irq_number,
                    "nvic_reg": f"ISER[{irq.irq_number // 32}]",
                    "nvic_bit": irq.irq_number % 32,
                    "peripheral_id": pid,
                })
                edges.append({
                    "source": pid,
                    "target": iid,
                    "type": "MAPS_TO_INTERRUPT",
                    "trigger_condition": "",
                })

        return {"nodes": nodes, "edges": edges}

    def extract_peripheral_report(self, name: str) -> dict:
        """Generate a human-readable report for manual verification."""
        periph = self.parse_peripheral(name)
        if not periph:
            return {"error": f"Peripheral '{name}' not found"}

        report = {
            "name": periph.name,
            "base_address": periph.base_address,
            "bus": periph.bus,
            "description": periph.description,
            "registers": [],
            "interrupts": [
                {"name": i.name, "irq_number": i.irq_number, "description": i.description}
                for i in periph.interrupts
            ],
        }

        for reg in periph.registers:
            reg_info = {
                "name": reg.name,
                "offset": reg.address_offset,
                "size": reg.size,
                "access": reg.access,
                "reset_value": reg.reset_value,
                "field_count": len(reg.fields),
                "fields": [
                    {
                        "name": f.name,
                        "bits": f"[{f.bit_start}:{f.bit_end}]",
                        "access": f.access,
                        "description": f.description,
                    }
                    for f in reg.fields
                ],
            }
            report["registers"].append(reg_info)

        return report
