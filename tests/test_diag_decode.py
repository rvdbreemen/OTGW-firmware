#!/usr/bin/env python3
"""TASK-1667: scripts/diag_decode.py against the REAL encoder (ADR-213 / ADR-214).

test/host/diag_fixture_gen.cpp includes src/OTGW-firmware/diagRecorderStuff.h as is, records a known
event sequence over several blocks and writes the download plus the expected decoded lines. This test
compiles it with MSVC (as the other test/host harnesses), decodes the recording and compares every
record and its uptime. A copy of the generated fixture is checked in under tests/fixtures/diag/, so the
decoder checks also run without a compiler; with a compiler the fresh output must match that copy byte
for byte (an encoder change then shows up here and the fixture is regenerated on purpose:
python tests/test_diag_decode.py --regen).
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "scripts"))
sys.path.insert(0, str(REPO / "test" / "host"))
import diag_decode as dd  # noqa: E402

FIX = REPO / "tests" / "fixtures" / "diag"
FIX_BIN, FIX_TXT = FIX / "recording-v1.bin", FIX / "recording-v1.expected.txt"
GEN_SRC = REPO / "test" / "host" / "diag_fixture_gen.cpp"
HEADER_DIR = REPO / "src" / "OTGW-firmware"


def build_generator(out_dir):
    """Compile the generator with MSVC; None when no MSVC is installed."""
    try:
        from test_ot_reserved_range import find_vcvars
        vcvars = find_vcvars()
    except Exception:
        return None
    if not vcvars:
        return None
    exe = out_dir / "diag_fixture_gen.exe"
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{vcvars}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{GEN_SRC}\" /I\"{HEADER_DIR}\"\r\n", encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        raise AssertionError("generator did not compile:\n" + c.stdout[-3000:] + c.stderr[-1000:])
    return exe


def generate(out_dir):
    exe = build_generator(out_dir)
    if exe is None:
        return None
    rec, txt = out_dir / "rec.bin", out_dir / "expected.txt"
    r = subprocess.run([str(exe), str(rec), str(txt)], capture_output=True, text=True)
    if r.returncode != 0:
        raise AssertionError("generator failed: " + r.stdout + r.stderr)
    return rec, txt


def expected(txt):
    rows = []
    for line in Path(txt).read_text(encoding="utf-8").splitlines():
        ms, s = line.split("\t", 1)
        rows.append((int(ms), s))
    return rows


def decoded(rec):
    _, _, records = dd.decode([rec])
    return [(r["ms"], dd.fmt(r)) for r in records]


class DecodeRoundTrip(unittest.TestCase):
    def test_checked_in_fixture_round_trips(self):
        self.assertEqual(decoded(FIX_BIN), expected(FIX_TXT))

    def test_fresh_fixture_from_the_real_encoder_round_trips(self):
        with tempfile.TemporaryDirectory() as d:
            out = generate(Path(d))
            if out is None:
                self.skipTest("no MSVC: the checked-in fixture covers the decoder")
            rec, txt = out
            self.assertEqual(decoded(rec), expected(txt))
            self.assertEqual(rec.read_bytes(), FIX_BIN.read_bytes(),
                             "the encoder output changed: regenerate with python tests/test_diag_decode.py --regen")

    def test_header_and_blocks(self):
        header, blocks, records = dd.decode([FIX_BIN])
        self.assertEqual((header["fw_version"], header["githash"], header["boot_count"]), ("2.0.0-test", "abc1234", 7))
        self.assertEqual(header["routes"], ["settings", "command"])
        self.assertTrue(header["psram"])
        seqs = [b["block_seq"] for b in blocks]
        self.assertGreaterEqual(len(seqs), 3)
        self.assertEqual(seqs, list(range(seqs[0], seqs[0] + len(seqs))))
        firsts = [b["first_record_seq"] for b in blocks]
        counts = [sum(1 for r in records if r["block"] == s) for s in seqs]
        self.assertEqual(firsts[1:], [firsts[i] + counts[i] for i in range(len(firsts) - 1)])
        rest = [r for r in records if r["kind"] == "rest_in"][0]
        self.assertEqual(rest["route_name"], "command")

    def test_a_decoder_that_misreads_dt_is_caught(self):
        # The comparison must be able to fail: read dt as one byte instead of a LEB128 varint.
        orig = dd.Reader.varint
        dd.Reader.varint = lambda self: self.u8() & 0x7F
        try:
            try:
                got = decoded(FIX_BIN)
            except dd.FormatError:
                return  # a mis-decode that derails is also a detected failure
            self.assertNotEqual(got, expected(FIX_TXT))
        finally:
            dd.Reader.varint = orig

    def test_names_label_the_ids(self):
        with tempfile.TemporaryDirectory() as d:
            names = Path(d) / "names.txt"
            names.write_text("boiler_flow_temp\nSATtargettemp\n", encoding="utf-8")
            _, _, records = dd.decode([FIX_BIN])
            lab = dd.load_names(names)
            lines = [dd.fmt(r, lab) for r in records if r["kind"] in ("mqtt_pub", "setting")]
            self.assertIn(f"MQ {dd.name_id('boiler_flow_temp'):04x}(boiler_flow_temp) 45.5", lines)
            self.assertIn(f"SET {dd.name_id('SATtargettemp'):04x}(SATtargettemp) 20.5", lines)

    def test_two_downloads_merge_by_block_seq(self):
        header, blocks, records = dd.decode([FIX_BIN, FIX_BIN])
        self.assertEqual(len(records), len(expected(FIX_TXT)))


class ReplayExport(unittest.TestCase):
    # ADR-209 decision 1: device_time TAB source TAB frame, device_time HH:MM:SS with 1-6 fraction digits.
    LINE = re.compile(r"^\d{2}:\d{2}:\d{2}\.\d{1,6}\t[TBRAE]\t[0-9A-F]{8}$")

    def test_otframes_lines_follow_adr_209(self):
        header, _, records = dd.decode([FIX_BIN])
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / "rec.log"
            n = dd.write_otframes(records, out, header)
            lines = out.read_text(encoding="ascii").splitlines()
            self.assertTrue(lines[0].startswith("#"))
            self.assertEqual(n, len(lines) - 1)
            self.assertEqual(n, sum(1 for r in records if r["kind"] == "ot"))
            for line in lines[1:]:
                self.assertRegex(line, self.LINE)

    def test_otframes_keep_the_recorded_gaps(self):
        header, _, records = dd.decode([FIX_BIN])
        ot = [r for r in records if r["kind"] == "ot"]
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / "rec.log"
            dd.write_otframes(records, out, header)
            times = [l.split("\t")[0] for l in out.read_text(encoding="ascii").splitlines()[1:]]

        def ms(t):
            h, m, s = t.split(":")
            return (int(h) * 3600 + int(m) * 60) * 1000 + round(float(s) * 1000)
        gaps_file = [ms(b) - ms(a) for a, b in zip(times, times[1:])]
        gaps_rec = [b["ms"] - a["ms"] for a, b in zip(ot, ot[1:])]
        self.assertEqual(gaps_file, gaps_rec)


class Diff(unittest.TestCase):
    def test_same_recording_has_no_difference(self):
        out, na, nb = dd.diff(dd.decode([FIX_BIN])[2], dd.decode([FIX_BIN])[2], {})
        self.assertEqual(out, [])
        self.assertEqual(na, nb)

    def test_a_changed_publish_shows_up(self):
        a = dd.decode([FIX_BIN])[2]
        b = [dict(r) for r in a]
        first = next(i for i, r in enumerate(b) if r["kind"] == "mqtt_pub")
        b[first]["value"] = "99.9"
        out, _, _ = dd.diff(a, b, {})
        self.assertTrue(any(l.startswith("+MQ") and "99.9" in l for l in out))


def regen():
    with tempfile.TemporaryDirectory() as d:
        out = generate(Path(d))
        if out is None:
            sys.exit("no MSVC found: cannot regenerate the fixture")
        FIX.mkdir(parents=True, exist_ok=True)
        FIX_BIN.write_bytes(out[0].read_bytes())
        FIX_TXT.write_bytes(out[1].read_bytes())
        print(f"wrote {FIX_BIN} ({FIX_BIN.stat().st_size} bytes) and {FIX_TXT}")


if __name__ == "__main__":
    if "--regen" in sys.argv:
        regen()
    else:
        unittest.main(verbosity=2)
