#!/usr/bin/env python3
"""TASK-1260: the manual's SAT MQTT command tables must match kSatMqttCmds[].

Parses kSatMqttCmds[] in MQTTstuff.ino and checks that
  - ch09 (EN) and h09 (NL) list exactly those command names under `sat/` (plus the three
    sub-token commands that live outside the table: area/{0-3}, zone/{1-4}/room_temp,
    zone/{1-4}/setpoint), and map every passthrough command to the same setting key;
  - docs/MANUAL.md 'SAT Command Topics' lists the same set;
  - the removed OVP calibration (ovp_*, SATovp*, OPV calibration) is not documented in
    ch05, h05, ch09 or h09.

OTGW_DOCS_REV=<git rev> reads the four manual chapters and MANUAL.md from that revision
instead of the working tree (old-vs-fix run: `OTGW_DOCS_REV=HEAD python
tests/test_manual_sat_commands.py` goes red on the docs before TASK-1260).

Run: python tests/test_manual_sat_commands.py
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
REV = os.environ.get("OTGW_DOCS_REV")

CH05 = "docs/manuals/en/ch05-sat-thermostat.md"
H05 = "docs/manuals/nl/h05-sat-thermostaat.md"
CH09 = "docs/manuals/en/ch09-api-reference.md"
H09 = "docs/manuals/nl/h09-api-referentie.md"
MANUAL = "docs/MANUAL.md"
MQTT_INO = "src/OTGW-firmware/MQTTstuff.ino"

# Sub-token commands handled outside kSatMqttCmds[] (see the comment above the table).
SUB_TOKEN = {"area/{0-3}", "zone/{1-4}/room_temp", "zone/{1-4}/setpoint"}


def read_doc(rel):
    if REV:
        out = subprocess.run(["git", "show", f"{REV}:{rel}"], cwd=REPO, capture_output=True, check=True)
        text = out.stdout.decode("utf-8")
    else:
        text = (REPO / rel).read_text(encoding="utf-8")
    return text.replace("\r\n", "\n")


def parse_firmware_table():
    """Return {command: settingKey or None} from kSatMqttCmds[]."""
    src = (REPO / MQTT_INO).read_text(encoding="utf-8")
    start = src.index("kSatMqttCmds[] PROGMEM = {")
    end = src.index("{ nullptr, nullptr, nullptr }", start)
    cmds = {}
    row = re.compile(r'^\s*\{\s*"([a-z0-9_]+)"\s*,\s*(nullptr|"[A-Za-z0-9]+")\s*,\s*(nullptr|\w+)\s*\}')
    for line in src[start:end].splitlines():
        m = row.match(line)
        if m:
            key = None if m.group(2) == "nullptr" else m.group(2).strip('"')
            cmds[m.group(1)] = key
    return cmds


def sat_section(text, begin_marker, end_marker):
    a = text.index(begin_marker)
    b = text.index(end_marker, a)
    return text[a:b]


def doc_rows(section):
    """Return ({suffix}, {suffix: setting}) from the `sat/...` table rows of a section."""
    names = set()
    settings = {}
    for line in section.splitlines():
        m = re.match(r"^\| `sat/([^`]+)` \|(.*)\|\s*$", line)
        if not m:
            continue
        names.add(m.group(1))
        cells = [c.strip() for c in m.group(2).split("|")]
        # the passthrough table has exactly one more cell: the setting key
        if len(cells) == 1 and re.fullmatch(r"`[A-Za-z0-9]+`", cells[0]):
            settings[m.group(1)] = cells[0].strip("`")
    return names, settings


class SatCommandDocs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fw = parse_firmware_table()
        cls.expected = set(cls.fw) | SUB_TOKEN

    def test_parser_found_the_table(self):
        self.assertGreater(len(self.fw), 70, "kSatMqttCmds[] parse looks incomplete")
        self.assertIn("overshoot_protection", self.fw)
        self.assertNotIn("ovp_enabled", self.fw)

    def _check_chapter(self, rel, begin, end):
        section = sat_section(read_doc(rel), begin, end)
        names, settings = doc_rows(section)
        self.assertEqual(sorted(self.expected - names), [], f"{rel}: commands missing from the doc")
        self.assertEqual(sorted(names - self.expected), [], f"{rel}: commands the firmware does not have")
        want = {c: k for c, k in self.fw.items() if k}
        self.assertEqual(settings, want, f"{rel}: passthrough setting keys differ from kSatMqttCmds[]")

    def test_ch09_en_matches_firmware(self):
        self._check_chapter(CH09, "##### SAT Command Topics", "##### OTGW32 Command Topics")

    def test_h09_nl_matches_firmware(self):
        self._check_chapter(H09, "**SAT-commando's:**", "**OTGW32-commando's")

    def test_manual_md_matches_firmware(self):
        self._check_chapter(MANUAL, "##### SAT Command Topics", "#### Home Assistant Auto-Discovery")

    def test_no_ovp_calibration_in_chapters(self):
        pattern = re.compile(r"ovp_|SATovp|\bOPV\b|Overshoot Protection Value", re.IGNORECASE)
        for rel in (CH05, H05, CH09, H09):
            hits = [f"{n}: {l.strip()[:90]}" for n, l in enumerate(read_doc(rel).splitlines(), 1) if pattern.search(l)]
            self.assertEqual(hits, [], f"{rel} still documents the removed OVP calibration")

    def test_ha_switch_list_has_no_ovp_and_names_real_commands(self):
        # every backticked name in the SAT switch row of the entity table must be a real command
        for rel in (CH05, H05):
            rows = [l for l in read_doc(rel).splitlines() if l.startswith("| `switch` |")]
            self.assertEqual(len(rows), 1, rel)
            cells = [c.strip() for c in rows[0].split("|")]
            names = re.findall(r"`([a-z_]+)`", cells[3])
            self.assertEqual(len(names), int(cells[2]), f"{rel}: switch count does not match the list")
            self.assertEqual([n for n in names if n not in self.fw], [], rel)


if __name__ == "__main__":
    unittest.main(verbosity=2)
