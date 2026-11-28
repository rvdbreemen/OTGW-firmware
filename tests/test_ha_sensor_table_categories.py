#!/usr/bin/env python3
"""TASK-2971 H1: the HA sensor table keeps protocol internals off the main Sensors list (ADR-140 section 2, no new ADR).

Rule over mqttHaSensors[] (MQTTHaDiscovery.cpp), checked for every row, so a new row that breaks it fails here:
  1. raw byte halves (label ending in _hb, _lb or _u8) and the status bytes (status_master, status_slave) are not
     "uncategorised and enabled by default": they carry entity_category diagnostic, and the ones moved by H1 are also
     disabled by default;
  2. SAT tuning internals (PID terms and output, Kp/Ki/Kd, raw derivative, error, duty/overshoot ratios, the 4 h
     averages) carry entity_category diagnostic.
Plus the payload: the real composer (test/host/test_ha_discovery_json.py) emits "entity_category":"diagnostic" and
"enabled_by_default":false for a moved byte-half row. Counts before/after go in the task notes; this test prints them.
Red on the base (OTGW_DISC_BASE, default 14539282e, the commit before H1; the pin works only while it stays reachable): rows break both rules there.
"""
import json
import os
import re
import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware" / "MQTTHaDiscovery.cpp"
BASE = os.environ.get("OTGW_DISC_BASE", "14539282e")   # the R1 release commit, the last revision before H1; origin/dev carries H1 once T2 lands. Works only while that commit stays reachable.
sys.path.insert(0, str(REPO / "test" / "host"))

ROW = re.compile(r"^\s*\{\s*(\d+),\s*[^,]+,\s*(ha_lbl_\w+),.*HaEntityCat::(\w+),\s*(true|false)", re.M)
SAT_INTERNAL = {"sat_pid_output", "sat_error", "sat_pid_p", "sat_pid_i", "sat_pid_d", "sat_raw_derivative",
                "sat_kp", "sat_ki", "sat_kd", "sat_duty_ratio", "sat_overshoot_fraction", "sat_overshoot_margin"}


# TASK-3151.3 (Robert 2026-11-28, one overshoot margin): the SAT_Overshoot_Margin row (id 252) left the table; the pinned base still
# has it. The base is compared without the retired rows, so "rows added, removed or relabelled" still catches every other change.
RETIRED_ROWS = {"sat_overshoot_margin"}


def table(text):
    i = text.index("const MqttHaSensorCfg PROGMEM mqttHaSensors[] = {")
    j = text.index("\n};", i)
    labels = dict(re.findall(r'const char (ha_lbl_\w+)\[\] PROGMEM = "([^"]*)";', text))
    return [(int(m.group(1)), m.group(2)[len("ha_lbl_"):], labels.get(m.group(2), ""), m.group(3), m.group(4) == "true")
            for m in ROW.finditer(text[i:j])]


def is_byte_or_status(label):
    return bool(re.search(r"(_hb|_lb|_u8)$", label, re.I)) or label in ("status_master", "status_slave")


def is_sat_internal(name):
    return name in SAT_INTERNAL or name.startswith("sat_4h_")


def violations(rows):
    bad = []
    for _id, name, label, cat, enabled in rows:
        if is_byte_or_status(label) and cat == "none" and enabled:
            bad.append(("byte/status uncategorised+enabled", label))
        if is_sat_internal(name) and cat != "diagnostic":
            bad.append(("SAT internal not diagnostic", name))
    return bad


class HaSensorTableCategories(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fix = table(SRC.read_text(encoding="utf-8"))
        old = subprocess.run(["git", "show", f"{BASE}:src/OTGW-firmware/MQTTHaDiscovery.cpp"], cwd=REPO,
                             capture_output=True, check=True).stdout.decode("utf-8")
        cls.base = [r for r in table(old) if r[1] not in RETIRED_ROWS]
        unc = lambda rows: sum(1 for r in rows if r[3] == "none" and r[4])
        print(f"\nrows {len(cls.fix)}; uncategorised+enabled: base {unc(cls.base)} -> fix {unc(cls.fix)}")

    def test_parser_sees_the_whole_table(self):
        self.assertEqual(len(self.fix), len(self.base))
        self.assertGreater(len(self.fix), 400)

    def test_rule_holds_for_every_row(self):
        self.assertEqual(violations(self.fix), [])

    def test_base_breaks_the_rule(self):
        self.assertTrue(violations(self.base), "the base already follows the rule: the red run proves nothing")

    def test_only_category_and_enabled_changed(self):
        self.assertEqual([r[:3] for r in self.fix], [r[:3] for r in self.base], "rows added, removed or relabelled")

    def test_payload_of_a_moved_row(self):
        import test_ha_discovery_json as H
        moved = [r[2] for r, b in zip(self.fix, self.base)
                 if is_byte_or_status(r[2]) and b[3] == "none" and b[4] and r[0] <= 127]
        self.assertTrue(moved)
        root = H.GEN / "ha_discovery_json" / "h1_fix"
        H.export(None, root)
        run = H.build_and_run("TASK-2971 H1 (working tree)", root, None)
        seen = 0
        for p in run[2]:
            if not p["raw"] or "/sensor/" not in p["topic"] or not p["topic"].endswith("/config"):
                continue
            d = json.loads(p["raw"].decode("utf-8"))
            if d.get("stat_t", "").rsplit("/", 1)[-1] in moved:
                seen += 1
                self.assertEqual(d.get("entity_category"), "diagnostic", p["topic"])
                self.assertIs(d.get("enabled_by_default"), False, p["topic"])
        self.assertGreater(seen, 0, "no payload found for a moved row")


if __name__ == "__main__":
    unittest.main(verbosity=2)
