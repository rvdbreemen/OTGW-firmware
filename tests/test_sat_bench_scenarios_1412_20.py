#!/usr/bin/env python3
"""TASK-1412.20: host test for the SAT bench-simulator scenario generator (scripts/sat_bench_scenarios_1412_20.py).

Checks, on freshly generated output:
  - every frame line has a source letter T/B/R/A and 8 hex digits with even parity (OpenTherm spec v4.2); the only other
    payload is a declared event line (the PIC banner);
  - device times rise monotonically, no gap exceeds the 60 s replay clamp (ADR-209), no midnight inside a fixture, and
    host_time and device_time agree;
  - each scenario's stimulus is really in its fixture (decoded: MsgID 3 HB bit 4, Data-Invalid / Unknown-DataID / missing
    answers on the named IDs, the silent window, the DHW, flame and fault bits, the banner);
  - every scenario has expected outputs, and every expected output cites a source that exists: a "## N." heading in
    docs/sat/sergeant-d-corrections.md, a "def name" in the named test file, the symbol in the named source file, an ADR
    file; an output without a source must be marked open with the question to ask;
  - the generator is deterministic and index.json matches the files;
  - the firmware's own replay parser and scheduler (cut from OTGW-Core.ino through tests/test_replay_timing.py, nothing
    re-implemented) reads each fixture as timed lines, hands the parser "<letter><payload>" in order and keeps the
    recorded gaps within 50 ms (ADR-209 decision contract). Skipped without a host C++ compiler.

Run: python tests/test_sat_bench_scenarios_1412_20.py
"""
import importlib.util
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
GEN_PATH = REPO / "scripts" / "sat_bench_scenarios_1412_20.py"
CORRECTIONS = REPO / "docs" / "sat" / "sergeant-d-corrections.md"


def load_gen():
    spec = importlib.util.spec_from_file_location("sat_bench_scenarios_1412_20", GEN_PATH)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


gen = load_gen()
LINE = re.compile(r"^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}\+00:00)\t(\d{2}):(\d{2}):(\d{2})\.(\d{6})\t(\S)\t(.+)$")


def parse_fixture(path: Path):
    rows = []
    for raw in path.read_text(encoding="ascii").splitlines():
        if raw.startswith("#"):
            continue
        m = LINE.match(raw)
        if not m:
            raise AssertionError("not an otframes.log line: %r" % raw)
        hh, mm, ss, us = (int(m.group(i)) for i in (2, 3, 4, 5))
        t = hh * 3600 + mm * 60 + ss + us / 1e6
        rows.append({"host": m.group(1), "t": t, "dev": "%02d:%02d:%02d.%06d" % (hh, mm, ss, us),
                     "letter": m.group(6), "payload": m.group(7)})
    return rows


def decode(payload):
    w = int(payload, 16)
    return {"type": (w >> 28) & 7, "id": (w >> 16) & 0xFF, "hb": (w >> 8) & 0xFF, "lb": w & 0xFF}


def frames(rows):
    return [dict(r, **decode(r["payload"])) for r in rows if re.fullmatch(r"[0-9A-F]{8}", r["payload"])]


def b_answers(rows, msg_id, t0=None, t1=None):
    return [f for f in frames(rows) if f["letter"] == "B" and f["id"] == msg_id
            and (t0 is None or f["t"] - gen.BASE_S >= t0) and (t1 is None or f["t"] - gen.BASE_S < t1)]


class Generated(unittest.TestCase):
    tmp = None
    out = None
    index = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tmp.name) / "a"
        cls.index = gen.generate(cls.out)
        cls.rows = {}
        for s in cls.index["scenarios"]:
            if s["fixture"]:
                cls.rows[s["id"]] = parse_fixture(cls.out / s["fixture"])
        cls.expected = {s["id"]: json.loads((cls.out / ("%s.expected.json" % s["id"])).read_text(encoding="ascii"))
                        for s in cls.index["scenarios"]}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    # ------------------------------------------------------------- frames and timing
    def test_every_frame_has_a_known_letter_and_even_parity(self):
        for sid, rows in self.rows.items():
            events = {e[1][1:] for e in next(s for s in gen.SCENARIOS if s["id"] == sid).get("events", ())}
            for r in rows:
                if r["payload"] in events:
                    continue
                self.assertIn(r["letter"], "TBRA", "%s: %r" % (sid, r))
                self.assertRegex(r["payload"], r"^[0-9A-F]{8}$", sid)
                self.assertTrue(gen.parity_ok(r["payload"]), "%s: odd parity %s" % (sid, r["payload"]))

    def test_parity_checker_catches_a_broken_frame(self):
        good = gen.frame(gen.READ_ACK, 3, 0x1004)
        self.assertTrue(gen.parity_ok(good))
        bad = "%08X" % (int(good, 16) ^ (1 << 31))
        self.assertFalse(gen.parity_ok(bad))
        # Known frames from the bench and the existing emulator (scripts/sat_boiler_emulator.py docstring).
        self.assertEqual(gen.frame(gen.READ_ACK, 3, 0x0004), "40030004")
        self.assertEqual(gen.frame(gen.READ_ACK, 0, 0x0000), "C0000000")

    def test_times_rise_stay_under_the_replay_clamp_and_match_host_time(self):
        for sid, rows in self.rows.items():
            self.assertGreater(len(rows), 10, sid)
            for a, b in zip(rows, rows[1:]):
                gap = b["t"] - a["t"]
                self.assertGreaterEqual(gap, 0, "%s: time goes back at %s" % (sid, b["dev"]))
                self.assertLessEqual(gap, gen.MAX_GAP_S, "%s: gap %.3f s above the 60 s clamp" % (sid, gap))
            self.assertLess(rows[-1]["t"], 86400, "%s crosses midnight" % sid)
            for r in rows:
                self.assertEqual(r["host"][11:23], r["dev"][:12], sid)

    def test_frame_cadence_is_one_exchange_per_second(self):
        for sid, rows in self.rows.items():
            ts = [r["t"] for r in rows if r["letter"] == "T"]
            for a, b in zip(ts, ts[1:]):
                self.assertAlmostEqual(b - a, gen.EXCHANGE_S, places=5, msg=sid)

    # ------------------------------------------------------------- stimulus content
    def test_support_and_veto_bit4(self):
        self.assertTrue(b_answers(self.rows["SB01-support"], 3))
        self.assertTrue(all(f["type"] == gen.READ_ACK and not f["hb"] & 0x10 for f in b_answers(self.rows["SB01-support"], 3)))
        self.assertTrue(all(f["type"] == gen.READ_ACK and f["hb"] & 0x10 for f in b_answers(self.rows["SB02-veto"], 3)))
        r = self.rows["SB03-veto-during-pwm"]
        self.assertTrue(all(not f["hb"] & 0x10 for f in b_answers(r, 3, 0, 240)))
        self.assertTrue(b_answers(r, 3, 240) and all(f["hb"] & 0x10 for f in b_answers(r, 3, 240)))
        self.assertTrue(all(f["type"] == gen.UNKNOWN_DATAID for f in b_answers(self.rows["SB04-id3-unknown"], 3)))

    def test_invalid_unknown_and_missing_answers_sit_in_their_windows(self):
        r = self.rows["SB05-id25-data-invalid"]
        self.assertTrue(all(f["type"] == gen.READ_ACK for f in b_answers(r, 25, 0, 60)))
        bad = b_answers(r, 25, 60, 180)
        self.assertTrue(bad and all(f["type"] == gen.DATA_INVALID for f in bad))
        self.assertTrue(all(f["type"] == gen.READ_ACK for f in b_answers(r, 25, 180)))
        self.assertTrue(all(f["type"] == gen.UNKNOWN_DATAID for f in b_answers(self.rows["SB06-id25-unknown"], 25, 60)))
        r = self.rows["SB07-id25-no-answer"]
        self.assertEqual(b_answers(r, 25, 60, 150), [])
        self.assertTrue(b_answers(r, 0, 60, 150), "other IDs keep answering")
        bad = b_answers(self.rows["SB08-id28-data-invalid"], 28, 30)
        self.assertTrue(bad and all(f["type"] == gen.DATA_INVALID for f in bad))
        self.assertGreaterEqual(bad[-1]["t"] - bad[0]["t"], 300, "Data-Invalid must last past the 5 minute mark")
        self.assertTrue(all(f["type"] == gen.UNKNOWN_DATAID for f in b_answers(self.rows["SB09-id28-unknown"], 28)))

    def test_link_silence_window_has_no_boiler_answer_for_45_s(self):
        r = self.rows["SB10-link-silence"]
        bs = [x["t"] - gen.BASE_S for x in r if x["letter"] == "B"]
        longest = max(b - a for a, b in zip(bs, bs[1:]))
        self.assertGreaterEqual(longest, 45.0)
        self.assertGreater(longest, 30.0 + 1.0, "longer than SAT_OT_LINK_LOST_MS (30 s)")
        self.assertTrue([x for x in r if x["letter"] == "T" and 60 <= x["t"] - gen.BASE_S < 105], "requests continue")

    def test_status_bits_for_ignition_fault_and_dhw(self):
        st = lambda sid, t0, t1: [f["lb"] for f in b_answers(self.rows[sid], 0, t0, t1)]
        self.assertTrue(all(not lb & 0x08 for lb in st("SB11-slow-ignition", 0, 150)))
        self.assertTrue(all(lb & 0x08 and lb & 0x02 for lb in st("SB11-slow-ignition", 150, 390)))
        self.assertTrue(all(lb & 0x01 and not lb & 0x08 for lb in st("SB12-lockout-fault", 60, 180)))
        self.assertTrue(all(not lb & 0x01 for lb in st("SB12-lockout-fault", 180, 240)))
        self.assertTrue(all(lb & 0x04 and lb & 0x08 for lb in st("SB13-dhw-draw", 60, 180)))
        self.assertTrue(all(not lb & 0x04 for lb in st("SB13-dhw-draw", 180, 510)))
        self.assertTrue(all(lb & 0x04 for lb in st("SB14-dhw-during-pwm", 300, 390)))

    def test_banner_line_is_present_once(self):
        r = self.rows["SB15-pic-restart-banner"]
        hits = [x for x in r if x["letter"] + x["payload"] == "OpenTherm Gateway 6.8"]
        self.assertEqual(len(hits), 1)
        self.assertAlmostEqual(hits[0]["t"] - gen.BASE_S, 60.5, places=5)

    # ------------------------------------------------------------- expected outputs and sources
    def test_every_scenario_has_expected_outputs_with_a_mode_and_label(self):
        ids = [s["id"] for s in gen.SCENARIOS]
        self.assertEqual(len(ids), len(set(ids)))
        for sid, doc in self.expected.items():
            self.assertIn(doc["mode"], ("replay-only", "satsim+replay", "wire-slave-required", "host-only"), sid)
            self.assertTrue(doc["expected_replay"] or doc["expected_wire"], "%s has no expected output" % sid)
            self.assertTrue(doc["does_not_prove"], "%s must say what it does not prove" % sid)
            self.assertEqual(doc["generator_version"], gen.VERSION)
            if doc["mode"] in ("replay-only", "satsim+replay"):
                self.assertEqual(doc["label"], "board-executed controller replay", sid)
                self.assertTrue(doc["fixture"] or doc["uses_fixture"], sid)

    def test_every_expected_output_cites_an_existing_source_or_is_open(self):
        headings = set(re.findall(r"^## (\d+)\.", CORRECTIONS.read_text(encoding="utf-8"), re.M))
        checked = 0
        for sid, doc in self.expected.items():
            for item in doc["expected_replay"] + doc["expected_wire"]:
                if item.get("open"):
                    self.assertEqual(item["value"], "expected: open, ask", sid)
                    self.assertTrue(item.get("ask"), "%s: an open expectation names its question" % sid)
                    continue
                self.assertTrue(item["sources"], "%s: %s has no source" % (sid, item["observable"]))
                for src in item["sources"]:
                    checked += 1
                    kind, ref = src["kind"], src["ref"]
                    if kind == "entry":
                        self.assertTrue(ref in headings, "%s: entry %s not in sergeant-d-corrections.md" % (sid, ref))
                    elif kind in ("test", "code"):
                        path, name = ref.split("::")
                        text = (REPO / path).read_text(encoding="utf-8")
                        pat = r"def %s\(" % re.escape(name) if kind == "test" else r"\b%s\b" % re.escape(name)
                        self.assertRegex(text, pat, "%s: %s not found in %s" % (sid, name, path))
                    elif kind == "adr":
                        self.assertTrue(list((REPO / "docs" / "adr").glob(ref + "-*.md")), "%s: %s missing" % (sid, ref))
                    else:
                        self.fail("%s: unknown source kind %s" % (sid, kind))
        self.assertGreater(checked, 40)

    def test_source_checker_rejects_a_made_up_citation(self):
        headings = set(re.findall(r"^## (\d+)\.", CORRECTIONS.read_text(encoding="utf-8"), re.M))
        self.assertNotIn("99999", headings)
        text = (REPO / gen.SLAVE).read_text(encoding="utf-8")
        self.assertNotRegex(text, r"def test_this_test_does_not_exist\(")

    def test_generator_is_deterministic_and_index_matches(self):
        out2 = Path(self.tmp.name) / "b"
        idx2 = gen.generate(out2)
        self.assertEqual(self.index, idx2)
        import hashlib
        for s in self.index["scenarios"]:
            if s["fixture"]:
                self.assertEqual(hashlib.sha256((self.out / s["fixture"]).read_bytes()).hexdigest(), s["sha256"])
                self.assertEqual((self.out / s["fixture"]).read_bytes(), (out2 / s["fixture"]).read_bytes())
                self.assertLess((self.out / s["fixture"]).stat().st_size, 200_000, "keep fixtures small for LittleFS")


class FirmwareReplayReadsTheFixtures(unittest.TestCase):
    """The real replay code from OTGW-Core.ino reads each fixture (harness from tests/test_replay_timing.py)."""
    exe = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        import test_replay_timing as rt
        from test_sat_pwm_override import compile_cpp
        cls.tmp = tempfile.TemporaryDirectory()
        work = Path(cls.tmp.name)
        text = rt.CORE_INO.read_text(encoding="utf-8")
        a, b = text.index(rt.START), text.index(rt.END)
        body = text[a:b].replace('#include "otgwSimulationFixture.h"', "")
        fixture = (REPO / "src" / "OTGW-firmware" / "otgwSimulationFixture.h").read_text(encoding="utf-8").replace("#pragma once", "")
        stubs = rt.STUBS.replace("g_ms < 1000 + 600000", "g_ms < 1000 + 3600000")   # fixtures run up to ~12 minutes
        assert stubs != rt.STUBS, "harness loop bound changed in test_replay_timing.py"
        cpp = work / "replay.cpp"
        cpp.write_text(stubs.replace("@BODY@", fixture + "\n" + body), encoding="utf-8")
        try:
            cls.exe = compile_cpp(cpp, work / "replay.exe")
        except subprocess.CalledProcessError as exc:
            raise AssertionError("host compile failed: %s" % exc)
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        cls.out = work / "fx"
        cls.index = gen.generate(cls.out)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_each_fixture_replays_in_order_with_its_recorded_gaps(self):
        worst = 0
        for s in self.index["scenarios"]:
            if not s["fixture"]:
                continue
            rows = parse_fixture(self.out / s["fixture"])
            res = subprocess.run([str(self.exe), str(self.out / s["fixture"]), str(len(rows))],
                                 capture_output=True, text=True, check=True).stdout
            got = []
            for line in res.splitlines():
                if line.startswith("mode="):
                    self.assertEqual(line, "mode=timed", s["id"])
                    continue
                t, txt = line.split("\t", 1)
                got.append((int(t), txt))
            self.assertEqual([g[1] for g in got], [r["letter"] + r["payload"] for r in rows], s["id"])
            t0_dev, t0_fw = rows[0]["t"], got[0][0]
            for r, g in zip(rows, got):
                err = abs((g[0] - t0_fw) - round((r["t"] - t0_dev) * 1000))
                worst = max(worst, err)
                self.assertLessEqual(err, 50, "%s: line at %s off by %d ms" % (s["id"], r["dev"], err))
        print("\n  firmware replay: worst cumulative timing error %d ms over all fixtures" % worst)


if __name__ == "__main__":
    unittest.main(verbosity=2)
