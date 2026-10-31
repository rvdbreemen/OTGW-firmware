#!/usr/bin/env python3
"""TASK-1276: Home Assistant discovery for the SAT power and energy topics.

SATcontrol.ino publishes sat/power (kW), sat/energy_total (kWh) and sat/energy_estimated_kwh
(kWh, estimated gas input). The manuals promise HA sensors for them, so MQTTHaDiscovery.cpp
must carry sensor rows for them. This is a source-level test: it parses the real
mqttHaSensors[] table and the real publishIfChangedF() calls, it does not re-implement them.

Checks:
  rows     each topic has a sensor row in the SAT core block (id 252, routed to the Sat device)
           with the right device_class, unit and state_class (HA Energy dashboard contract);
  topics   the row label equals the topic SATcontrol.ino publishes, so stat_t matches;
  index    MQTT_HA_SENSOR_COUNT equals the row count, mqttHaSensorIndex[] points at the first row
           of every id and every id is contiguous. Adding rows to a block shifts later ids; a
           stale index entry makes the discovery loop stop at the wrong row and silently drops
           that whole id.

Run: python tests/test_sat_energy_discovery.py
"""
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"

# topic -> (device_class, unit, state_class, retained)
EXPECTED = {
    "sat/power":                ("power",  "kW",  "measurement",      False),
    "sat/energy_total":         ("energy", "kWh", "total_increasing", True),
    "sat/energy_estimated_kwh": ("energy", "kWh", "total_increasing", True),
}

ROW = re.compile(
    r"^\s*\{\s*(\d+),\s*(0x[0-9A-Fa-f]+),\s*(\w+),\s*(\w+),\s*HaDeviceClass::(\w+),\s*HaUnit::(\w+),"
    r"\s*HaStateClass::(\w+),\s*HaIcon::(\w+),\s*HaEntityCat::(\w+),\s*(true|false)")
DECL = re.compile(r'^DECLARE_SAT_DISCOVERY_STRINGS\((\w+),\s*"([^"]+)",\s*"([^"]+)"\)', re.M)


def read(name):
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


def sensor_table(ha):
    start = ha.index("const MqttHaSensorCfg PROGMEM mqttHaSensors[] = {")
    end = ha.index("};", start)
    rows = []
    for line in ha[start:end].splitlines():
        m = ROW.match(line)
        if m:
            rows.append({"id": int(m.group(1)), "flags": int(m.group(2), 16), "lbl": m.group(3),
                         "name": m.group(4), "dc": m.group(5), "unit": m.group(6),
                         "sc": m.group(7), "icon": m.group(8), "cat": m.group(9),
                         "enabled": m.group(10) == "true"})
    return rows


def sensor_index(ha):
    start = ha.index("const uint16_t PROGMEM mqttHaSensorIndex[256] = {")
    end = ha.index("};", start)
    vals = re.findall(r"^\s*(0xFFFF|\d+),?\s*//\s*id (\d+)\b", ha[start:end], re.M)
    return {int(i): (0xFFFF if v == "0xFFFF" else int(v)) for v, i in vals}


class SatEnergyDiscovery(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ha = read("MQTTHaDiscovery.cpp")
        cls.sat = read("SATcontrol.ino")
        cls.rows = sensor_table(cls.ha)
        cls.decl = {k: (t, n) for k, t, n in DECL.findall(cls.ha)}   # key -> (topic, name)

    def row_for(self, topic):
        hits = [r for r in self.rows if r["lbl"].startswith("ha_lbl_") and
                self.decl.get(r["lbl"][len("ha_lbl_"):], ("",))[0] == topic]
        return hits

    def test_each_topic_is_published_by_satcontrol(self):
        for topic, (_, _, _, retained) in EXPECTED.items():
            with self.subTest(topic):
                m = re.search(r'publishIfChangedF\(F\("%s"\),[^;]*?,\s*(true|false)\);' % re.escape(topic), self.sat)
                self.assertIsNotNone(m, "SATcontrol.ino no longer publishes " + topic)
                self.assertEqual(m.group(1) == "true", retained)

    def test_each_topic_has_exactly_one_sat_core_sensor_row(self):
        for topic, (dc, unit, sc, _) in EXPECTED.items():
            with self.subTest(topic):
                hits = self.row_for(topic)
                self.assertEqual(len(hits), 1, "expected one sensor row for " + topic)
                r = hits[0]
                self.assertEqual(r["id"], 252, "must sit in the SAT core block (routes to the Sat device)")
                self.assertEqual((r["dc"], r["unit"], r["sc"]), (dc, unit, sc))
                self.assertEqual(r["cat"], "none")       # a primary entity, not diagnostic
                self.assertTrue(r["enabled"])
                self.assertEqual(r["flags"], 0)          # plain value topic: no source variants, no pic prefix

    def test_names_follow_the_sat_convention_and_are_unique(self):
        names = []
        for topic in EXPECTED:
            r = self.row_for(topic)[0]
            key = r["lbl"][len("ha_lbl_"):]
            self.assertTrue(self.decl[key][1].startswith("SAT_"), topic)
            names.append(self.decl[key][1])
        self.assertEqual(names[:2], ["SAT_Power", "SAT_Energy_Total"])   # sensor.otgw_sat_power / _energy_total in the manuals
        every_name = [n for _, n in self.decl.values()]
        for n in names:
            self.assertEqual(every_name.count(n), 1, n + " is declared twice")

    def test_count_matches_the_row_count(self):
        m = re.search(r"const uint16_t MQTT_HA_SENSOR_COUNT = (\d+);", self.ha)
        self.assertEqual(int(m.group(1)), len(self.rows))

    def test_index_points_at_the_first_row_of_every_id(self):
        index = sensor_index(self.ha)
        first = {}
        for i, r in enumerate(self.rows):
            first.setdefault(r["id"], i)
        seen_ids = set()
        for i, r in enumerate(self.rows):           # contiguity: an id never reappears after another id
            if i and r["id"] != self.rows[i - 1]["id"]:
                self.assertNotIn(r["id"], seen_ids, "id %d is not contiguous" % r["id"])
            seen_ids.add(r["id"])
        self.assertEqual(len(index), 256)
        for oid, pos in index.items():
            with self.subTest(id=oid):
                if oid in first:
                    self.assertEqual(pos, first[oid], "mqttHaSensorIndex[%d] is stale" % oid)
                else:
                    self.assertEqual(pos, 0xFFFF)

    def test_cycles_this_hour_is_a_measurement_not_a_meter(self):
        # TASK-1463: the value falls back as starts leave the rolling hour, so total_increasing
        # would make HA read every drop as a meter reset.
        hits = self.row_for("sat/cycles_this_hour")
        self.assertEqual(len(hits), 1)
        self.assertEqual(hits[0]["sc"], "measurement")
        self.assertEqual(hits[0]["id"], 252)


if __name__ == "__main__":
    unittest.main()
