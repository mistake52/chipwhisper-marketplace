"""
SVD Parser for STM32F1 — thin wrapper around cmsis-svd library.

Converts CMSIS-SVD parsed data into PRD-defined node/edge graph format
(Section 4 data model) with peripheral whitelist filtering.
"""

from dataclasses import dataclass, field, asdict
from typing import Optional

from cmsis_svd.parser import SVDParser as CmsisSVDParser


# Phase 0 peripheral whitelist (case-insensitive, GPIO is prefix match)
PERIPHERAL_WHITELIST = {
    "RCC", "GPIO",
    "USART1", "USART2",
    "SPI1", "I2C1",
    "TIM2", "ADC1", "DMA1",
    "FLASH", "NVIC",
}

PREFIX_PATTERNS = {"GPIO"}

# Bus mapping based on STM32F1 memory layout
APB1_PERIPHERALS = {"TIM2", "USART2", "SPI2", "I2C1", "I2C2"}
APB2_PERIPHERALS = {"USART1", "SPI1", "ADC1", "ADC2", "ADC3", "GPIOA", "GPIOB",
                    "GPIOC", "GPIOD", "GPIOE", "GPIOF", "GPIOG", "AFIO"}
AHB_PERIPHERALS = {"DMA1", "DMA2", "FLASH", "RCC", "CRC", "FSMC", "SDIO"}


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


def _get_bus(periph_name: str) -> str:
    """Determine bus domain from peripheral name (STM32F1 memory map)."""
    upper = periph_name.upper()
    if any(upper == p for p in AHB_PERIPHERALS) or upper.startswith("DMA"):
        return "AHB"
    for p in APB1_PERIPHERALS:
        if upper == p or upper.startswith(p):
            return "APB1"
    for p in APB2_PERIPHERALS:
        if upper == p:
            return "APB2"
    if upper.startswith("GPIO") or upper == "AFIO":
        return "APB2"
    return "APB1"


def _is_whitelisted(name: str) -> bool:
    """Check if peripheral name matches Phase 0 whitelist (case-insensitive)."""
    upper = name.upper()
    for pattern in PREFIX_PATTERNS:
        if upper.startswith(pattern):
            return True
    return upper in PERIPHERAL_WHITELIST


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
    """Parse STM32F1 SVD file and produce PRD-compliant graph data."""

    def __init__(self, svd_path: str):
        self.svd_path = svd_path
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

        for cmsis_periph in self._device.peripherals:
            if not _is_whitelisted(cmsis_periph.name):
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
        bus = _get_bus(cmsis_periph.name)

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

        for periph in self._parsed_peripherals:
            pid = f"F103_{periph.name}"

            # Peripheral node
            nodes.append({
                "id": pid,
                "type": "Peripheral",
                "name": periph.name,
                "base_addr": periph.base_address,
                "bus": periph.bus,
                "version": "v1",
                "chip_id": "STM32F103C8",
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
