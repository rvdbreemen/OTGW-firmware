#!/usr/bin/env python3
"""TASK-2971 H3: OpenTherm binary sensors carry a Home Assistant device_class (ADR-140 section 3, no new ADR).

Fault rows announce device_class problem, the boiler/thermostat/OTGW connected rows connectivity, and the flame,
central heating, hot water and cooling activity rows running. HA then shows "Problem / OK", "Connected /
Disconnected" and "Running / Not running" instead of a bare "On / Off".

The payloads come from the REAL composers: test/host/test_ha_discovery_json.py compiles MQTTHaDiscovery.cpp
verbatim with the MQTTstuff.ino slices it needs and records every publish. This test builds that program twice,
from the working tree and from OTGW_DISC_BASE (default 890b08680, the parent of the change). The pin works only while that commit stays reachable (never rewrite or drop it from origin/dev history); a default of origin/dev stopped working once the change landed., and checks:
  1. the rows above carry the expected device_class (red on the base, which has none);
  2. payload_on matches the class: these rows use HA's default ON/OFF payloads (no pl_on/pl_off keys), and the
     firmware publishes ON when the fault / connection / activity is present (CCONOFF, MQTTstuff.ino);
  3. contract unchanged (architect's condition): every binary_sensor discovery topic, uniq_id and stat_t is the
     same as on the base; only the device_class key may differ.

Run:  python tests/test_ha_binary_device_class.py
"""
import json
import os
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "test" / "host"))
import test_ha_discovery_json as H   # noqa: E402

BASE = os.environ.get("OTGW_DISC_BASE", "890b08680")   # the last commit before the change under test; origin/dev carries it since the R1 release

EXPECT = {
    "problem": ["fault", "gas_flame_fault", "air_pressure_fault", "low_water_pressure", "water_over_temperature",
                "service_request", "vh_fault", "solar_storage_slave_fault_indicator"],
    "connectivity": ["boiler_connected", "thermostat_connected", "otgw_connected"],
    "running": ["flame", "centralheating", "domestichotwater", "cooling"],
}


def binaries(run):
    """{discovery topic: payload dict} for every binary_sensor config publish."""
    out = {}
    for p in run[2]:
        if not p["raw"] or "/binary_sensor/" not in p["topic"] or not p["topic"].endswith("/config"):
            continue
        out[p["topic"]] = json.loads(p["raw"].decode("utf-8"))
    return out


def by_label(pay):
    """label -> payload, from the state topic tail (OTGW/value/<node>/<label>)."""
    out = {}
    for d in pay.values():
        tail = d.get("stat_t", "").rsplit("/", 1)[-1]
        out.setdefault(tail, d)
    return out


class HaBinaryDeviceClass(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = H.GEN / "ha_discovery_json"
        H.export(None, root / "devcls_fix")
        cls.fix = binaries(H.build_and_run("TASK-2971 H3 (working tree)", root / "devcls_fix", None))
        H.export(BASE, root / "devcls_base")
        cls.base = binaries(H.build_and_run(f"TASK-2971 H3 ({BASE})", root / "devcls_base", BASE))

    def test_rows_carry_the_device_class(self):
        lab = by_label(self.fix)
        for cls_, labels in EXPECT.items():
            for name in labels:
                with self.subTest(name):
                    self.assertIn(name, lab, f"no binary_sensor discovery for {name}")
                    self.assertEqual(lab[name].get("device_class"), cls_, name)

    def test_payload_is_the_ha_default_on_off(self):
        lab = by_label(self.fix)
        for labels in EXPECT.values():
            for name in labels:
                with self.subTest(name):
                    self.assertNotIn("pl_on", lab[name])
                    self.assertNotIn("payload_on", lab[name])

    def test_topics_uniq_id_and_state_topic_unchanged(self):
        self.assertEqual(sorted(self.fix), sorted(self.base), "binary_sensor discovery topics changed")
        changed = []
        for topic, new in self.fix.items():
            old = self.base[topic]
            for key in ("uniq_id", "stat_t", "avty_t", "name"):
                if new.get(key) != old.get(key):
                    changed.append((topic, key, old.get(key), new.get(key)))
            extra = set(new) ^ set(old)
            if extra - {"device_class"}:
                changed.append((topic, "keys", sorted(set(old) - set(new)), sorted(set(new) - set(old))))
        self.assertEqual(changed, [], "only device_class may differ from the base")

    def test_base_had_no_device_class_on_these_rows(self):
        lab = by_label(self.base)
        had = [n for labels in EXPECT.values() for n in labels if lab.get(n, {}).get("device_class")]
        self.assertEqual(had, [], "the base already had a device_class: the red run proves nothing")


if __name__ == "__main__":
    unittest.main(verbosity=2)
