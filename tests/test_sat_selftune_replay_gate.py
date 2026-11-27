#!/usr/bin/env python3
"""Regression gate for the SAT heating-curve self-tune on REAL captured data (TASK-3106, proposal P4).

The REAL pure tuner block of SATcontrol.ino (satCurveTune begin/end) and SATcurveTune.h are compiled on the host and fed the per-tick
streams of three captures (indigo_light, marceld91d, will_85903, all beta.428) stored as small anonymised fixtures in
tests/data/selftune_replay/ (numbers only, built by scripts/sat_selftune_fixture.py). The gate checks:
  1. the baseline reproduces the capture's OWN hourly records: every hour counted or not counted exactly as the device recorded it;
  2. the golden counts and decisions of the baseline and of the Q4=C variant (min_mod only when the room is not under target), as
     measured in selftune_replay/REPORT.md: indigo 4 counted hours and no decision, 14 and a raise 2.30 -> 2.35 at hour 16 with
     the variant; marcel 15/15, will 0/0, no decisions;
  3. fixture hygiene: gzip size bounds, only numbers and reason words, no dotted quads, no '@', no host-like text;
  4. the gate itself is sensitive: two mutants of the tuner (default minimum modulation 5 -> 1, decision band 1.0 -> 0.2) make it fail.

A legitimate tuner change makes this test fail on purpose: re-run the replay, review the new numbers, and update GOLDEN and the fixtures in the same commit.
Without a host C++ compiler the tests are skipped (they do not pass silently).

Run: python tests/test_sat_selftune_replay_gate.py
"""
import gzip
import re
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
sys.path.insert(0, str(REPO / "tests"))
import sat_selftune_replay as R  # noqa: E402

FW = REPO / "src" / "OTGW-firmware"
FIX = REPO / "tests" / "data" / "selftune_replay"
NAMES = ("indigo", "marcel", "will")
# counted hours and decisions (hour, direction, new coefficient) per capture and variant
GOLDEN = {
    "indigo": {"base": (4, []), "under": (14, [(16, 1, 2.35)])},
    "marcel": {"base": (15, []), "under": (15, [])},
    "will":   {"base": (0, []), "under": (0, [])},
}


def load(name):
    text = gzip.open(FIX / ("%s.txt.gz" % name), "rt", encoding="ascii").read()
    coeff = float(re.search(r"^C ([\d.]+)$", text, re.M).group(1))
    recorded = {int(m.group(1)): int(m.group(2)) for m in re.finditer(r"^R (\d+) (\d) \S+$", text, re.M)}
    return text, coeff, recorded


def gate_failures(exe_base, exe_under):
    """Everything this gate checks, as a list of failure strings."""
    bad = []
    for name in NAMES:
        text, coeff, recorded = load(name)
        for variant, exe in (("base", exe_base), ("under", exe_under)):
            rows = R.run(exe, text, coeff)
            counted = sum(r["counted"] for r in rows)
            decs = [(r["hour"], r["decision"], r["newcoeff"]) for r in rows if r.get("decision")]
            want_n, want_d = GOLDEN[name][variant]
            if counted != want_n:
                bad.append("%s %s: counted %d, golden %d" % (name, variant, counted, want_n))
            if decs != want_d:
                bad.append("%s %s: decisions %s, golden %s" % (name, variant, decs, want_d))
            if variant == "base":
                diff = [i for i, r in enumerate(rows) if recorded.get(i) is not None and recorded[i] != r["counted"]]
                if diff:
                    bad.append("%s base: hours %s differ from the capture's own record" % (name, diff))
    return bad


class SelfTuneReplayGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.dir = Path(cls.tmp.name)
        try:
            cls.exe_base = R.compile_driver(str(FW / "SATcontrol.ino"), str(FW / "SATcurveTune.h"), "base", cls.dir)
            cls.exe_under = R.compile_driver(str(FW / "SATcontrol.ino"), str(FW / "SATcurveTune.h"), "under", cls.dir)
        except SystemExit:
            raise unittest.SkipTest("no host C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_fixtures_are_small_and_anonymous(self):
        total = 0
        for name in NAMES:
            f = FIX / ("%s.txt.gz" % name)
            size = f.stat().st_size
            total += size
            self.assertLess(size, 50 * 1024, "%s fixture is %d bytes gz (budget 50 KB)" % (name, size))
            text = gzip.open(f, "rt", encoding="ascii").read()   # ascii: no names with accents either
            for ln in text.splitlines():
                self.assertRegex(ln, r"^(# [\w ',.():-]+|C [\d.]+|R \d+ \d [a-z_]+|T [-\d. e+]+|M \d+|E \d+)$", "unexpected fixture line: %r" % ln[:80])
            self.assertIsNone(re.search(r"\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b", text), "an IP-like number in %s" % name)
            self.assertNotIn("@", text)
            self.assertNotRegex(text, r"(?i)\.local|otgw-|ssid|mac|latitude|longitude")
        self.assertLess(total, 150 * 1024)

    def test_baseline_reproduces_the_captures_hourly_records_and_the_golden_numbers(self):
        self.assertEqual(gate_failures(self.exe_base, self.exe_under), [])

    def _mutant(self, tag, old, new):
        hdr = (FW / "SATcurveTune.h").read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertIn(old, hdr, "mutation anchor not found: the tuner header changed")
        p = self.dir / ("mut_%s.h" % tag)
        p.write_text(hdr.replace(old, new), encoding="utf-8")
        b = R.compile_driver(str(FW / "SATcontrol.ino"), str(p), "base", self.dir)
        u = R.compile_driver(str(FW / "SATcontrol.ino"), str(p), "under", self.dir)
        return gate_failures(b, u)

    def test_mutant_default_minimum_modulation_is_caught(self):
        bad = self._mutant("minmod", "#define SAT_CT_DEFAULT_MIN_MOD  5.0f", "#define SAT_CT_DEFAULT_MIN_MOD  1.0f")
        self.assertTrue(bad, "the gate did not notice a default minimum modulation of 1 instead of 5")

    def test_mutant_decision_band_is_caught(self):
        bad = self._mutant("band", "#define SAT_CT_BAND_C           1.0f", "#define SAT_CT_BAND_C           0.2f")
        self.assertTrue(bad, "the gate did not notice a decision band of 0.2 instead of 1.0")


if __name__ == "__main__":
    unittest.main(verbosity=2)
