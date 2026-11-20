"""docs/api/MQTT.md states the HA discovery table sizes that MQTTHaDiscovery.cpp defines (TASK-2974).

The Entity Inventory table in MQTT.md names MQTT_HA_SENSOR_COUNT, MQTT_HA_BINSENSOR_COUNT and the
number of indexed binary-sensor rows. When a row is added to the discovery table, the constant
changes and this test fails until the doc follows. It also guards the device layout: the doc must
describe the single-device topology of ADR-140, not the superseded five-device layout of ADR-122.

Run: python tests/test_mqtt_doc_entity_counts.py   (or MQTT_DOC=<path> to check another copy)
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CPP = REPO / "src" / "OTGW-firmware" / "MQTTHaDiscovery.cpp"
DOC = Path(os.environ.get("MQTT_DOC", REPO / "docs" / "api" / "MQTT.md"))


def cpp_const(name: str) -> int:
    m = re.search(r"const\s+uint16_t\s+" + name + r"\s*=\s*(\d+)\s*;", CPP.read_text(encoding="utf-8"))
    assert m, f"{name} not found in {CPP}"
    return int(m.group(1))


class MqttDocEntityCounts(unittest.TestCase):
    def setUp(self):
        self.doc = DOC.read_text(encoding="utf-8")

    def test_sensor_count(self):
        n = cpp_const("MQTT_HA_SENSOR_COUNT")
        self.assertTrue(f"| `sensor` | {n} (`MQTT_HA_SENSOR_COUNT`)" in self.doc, "MQTT.md lacks: " + f"| `sensor` | {n} (`MQTT_HA_SENSOR_COUNT`)")

    def test_binary_sensor_count(self):
        n = cpp_const("MQTT_HA_BINSENSOR_COUNT")
        self.assertTrue(f"| `binary_sensor` | {n} (`MQTT_HA_BINSENSOR_COUNT`)" in self.doc, "MQTT.md lacks: " + f"| `binary_sensor` | {n} (`MQTT_HA_BINSENSOR_COUNT`)")

    def test_binary_sensor_indexed_count(self):
        n = cpp_const("MQTT_HA_BINSENSOR_INDEXED_COUNT")
        self.assertTrue(f"(first {n} rows indexed" in self.doc, "MQTT.md lacks: " + f"(first {n} rows indexed")

    def test_single_device_topology(self):
        self.assertTrue("## HA Discovery: one device per gateway (ADR-140)" in self.doc, "MQTT.md lacks: " + "## HA Discovery: one device per gateway (ADR-140)")
        self.assertFalse("Five-Device Topology" in self.doc, "MQTT.md still describes the ADR-122 five-device layout")


if __name__ == "__main__":
    unittest.main(verbosity=2)
