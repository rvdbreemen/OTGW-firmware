#!/usr/bin/env python3
"""TASK-1403: host test for ADR-201, the combo board detection persists only a positive PIC verdict.

The code under test is NOT re-implemented here. The test cuts two real blocks out of the sources
and compiles them with a host C++ compiler (g++, clang++ or MSVC cl via vswhere):

  1. src/OTGW-firmware/OTGW-firmware.ino, the whole ADR-127 board resolution inside
     `#if HAS_RUNTIME_HW_DETECT` of setup(): from `if (settings.iBoardMode == 2) {` up to the
     closing brace before `applyResolvedComboPins();` (forced OT-Direct recovery, forced PIC,
     auto detection with its 3 retries, and the `if (settings.iBoardMode != 0) writeSettings(false)`).
  2. src/OTGW-firmware/OTGW-Core.ino, the banner re-detect safety net: the
     `if (!state.pic.bAvailable && !isOTDirectEnabled()) { ... }` block of the PIC banner handler.

Stubs only replace hardware and side effects: the PIC serial object, detectPIC() (reports a
scripted PIC verdict), initOTDirect(), writeSettings() (records iBoardMode at the moment of the
write), logging and delay(). Without a compiler the tests are skipped, they do not pass silently.

OTGW_INO and OTGW_CORE_INO point at other copies of the two sources (old-vs-fix runs). The
MutantGate case generates a mutant at test time (the no-PIC branch persists OT-Direct = 2) and
asserts that the same checks go red on it, so the gate proves itself on every run.

Run: python tests/test_board_detect_persist_gate.py
"""
import itertools
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402  (compile_cpp: g++, clang++ or MSVC)

REPO = Path(__file__).resolve().parent.parent
INO = Path(os.environ.get("OTGW_INO", REPO / "src" / "OTGW-firmware" / "OTGW-firmware.ino"))
CORE_INO = Path(os.environ.get("OTGW_CORE_INO", REPO / "src" / "OTGW-firmware" / "OTGW-Core.ino"))

START_MARK = "  if (settings.iBoardMode == 2) {"
END_MARK = "  // The resolved mode may select the other pin map"
NOPIC_LINE = "initOTDirect();            // sets HW_MODE_OT_DIRECT for this boot only"

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(x) x
#define PSTR(x) x
#define SetupDebugf(...) ((void)0)
#define SetupDebugln(x) ((void)0)
#define DebugTln(x) ((void)0)
#define PIN_PIC_RX 1
#define PIN_PIC_TX 2
#define SERIAL_8N1 0
struct HwState { bool bClassicPro = false; int eMode = 0; };
struct PicState { bool bAvailable = false; };
struct State { HwState hw; PicState pic; };
struct Settings { int iBoardMode = 0; };
static State state; static Settings settings;
enum { HW_MODE_PIC = 1 };
static bool g_pic = false;                  // scripted verdict of the PIC probe
static int g_detects = 0, g_writes = 0, g_otdirect = 0;
static int g_writeValues[8]; static int g_nWrites = 0;
static int g_sawTwoWrite = 0;               // a write that stored iBoardMode == 2
static void detectPIC() { g_detects++; }
static bool isPICEnabled() { return g_pic; }
static bool isOTDirectEnabled() { return false; }
static void initOTDirect() { g_otdirect++; }
static void delay(int) {}
static int activePicRst() { return 0; }
static int activeLed2() { return 0; }
static void writeSettings(bool) {
  g_writes++; if (g_nWrites < 8) g_writeValues[g_nWrites++] = settings.iBoardMode;
  if (settings.iBoardMode == 2) g_sawTwoWrite = 1;
}
struct SerialStub { void begin(int, int, int, int) {} void setResetPin(int) {} void setProgressLed(int) {} void end() {} };
static SerialStub OTGWSerial;
"""

MAIN = r"""
int main(int argc, char **argv) {
  // argv: where(0=setup resolution, 1=banner handler) mode pic pro
  int where = atoi(argv[1]); settings.iBoardMode = atoi(argv[2]); g_pic = atoi(argv[3]) != 0;
  state.hw.bClassicPro = atoi(argv[4]) != 0;
  if (where == 0) resolveBoard(); else bannerHandler();
  printf("mode=%d writes=%d detects=%d otdirect=%d saw2=%d pic=%d", settings.iBoardMode, g_writes, g_detects,
         g_otdirect, g_sawTwoWrite, state.pic.bAvailable ? 1 : 0);
  printf(" values=");
  for (int i = 0; i < g_nWrites; i++) printf("%d,", g_writeValues[i]);
  printf("\n");
  return 0;
}
"""


def cut_setup(src: str) -> str:
    a = src.index(START_MARK)
    b = src.index(END_MARK, a)
    block = src[a:b]
    # The cut starts after `#if HAS_RUNTIME_HW_DETECT`; check that anchor really precedes it.
    assert "#if HAS_RUNTIME_HW_DETECT" in src[max(0, a - 120):a], "HAS_RUNTIME_HW_DETECT anchor moved"
    return "static void resolveBoard() {\n" + block + "}\n"


def cut_core(src: str) -> str:
    a = src.index("    if (!state.pic.bAvailable && !isOTDirectEnabled()) {")
    b = src.index("    strlcpy(state.pic.sFwversion", a)
    block = src[a:b]
    assert "writeSettings(false)" in block, "banner persist site moved"
    return "#define HAS_RUNTIME_HW_DETECT 1\nstatic void bannerHandler() {\n" + block + "}\n"


def load(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


_exe_cache = {}


def build(workdir: Path, setup_src: str, core_src: str, tag: str):
    key = (tag, hash(setup_src), hash(core_src))
    if key in _exe_cache:
        return _exe_cache[key]
    text = ("#define HAS_RUNTIME_HW_DETECT 1\n" + STUBS + cut_setup(setup_src) + "\n" + cut_core(core_src) + MAIN)
    # the core cut defines the macro itself; avoid the redefinition warning-as-error on strict compilers
    text = text.replace("#define HAS_RUNTIME_HW_DETECT 1\nstatic void bannerHandler", "static void bannerHandler")
    cpp = workdir / (tag + ".cpp")
    cpp.write_text(text, encoding="utf-8")
    exe = base.compile_cpp(cpp, workdir / (tag + (".exe" if os.name == "nt" else "")))
    _exe_cache[key] = exe
    return exe


def run(exe, where, mode, pic, pro=0):
    out = subprocess.run([str(exe), str(where), str(mode), str(pic), str(pro)], capture_output=True, text=True,
                         check=True).stdout.strip()
    d = dict(kv.split("=", 1) for kv in out.split())
    d = {k: v for k, v in d.items()}
    return {"mode": int(d["mode"]), "writes": int(d["writes"]), "detects": int(d["detects"]),
            "otdirect": int(d["otdirect"]), "saw2": int(d["saw2"]), "pic": int(d["pic"]),
            "values": [int(x) for x in d.get("values", "").split(",") if x]}


def check_contract(tc, exe):
    """The ADR-201 contract; returns nothing, asserts on tc."""
    # 1. auto + PIC found -> Classic (1, or 3 for the Pro) persisted
    for pro, want in ((0, 1), (1, 3)):
        r = run(exe, 0, 0, 1, pro)
        tc.assertEqual(r["mode"], want, r)
        tc.assertEqual(r["writes"], 1, r)
        tc.assertEqual(r["values"], [want], r)
        tc.assertEqual(r["otdirect"], 0, r)
    # 2. auto + no PIC -> OT-Direct this boot, iBoardMode stays 0, nothing persisted, probed 3 times
    r = run(exe, 0, 0, 0)
    tc.assertEqual(r["mode"], 0, r)
    tc.assertEqual(r["writes"], 0, r)
    tc.assertEqual(r["otdirect"], 1, r)
    tc.assertEqual(r["detects"], 3, r)
    # 3. forced 2 + PIC found -> corrected to Classic and persisted
    for pro, want in ((0, 1), (1, 3)):
        r = run(exe, 0, 2, 1, pro)
        tc.assertEqual(r["mode"], want, r)
        tc.assertEqual(r["values"], [want], r)
        tc.assertEqual(r["otdirect"], 0, r)
    #    forced 2 + no PIC -> stays 2 (a user force), OT-Direct, no write
    r = run(exe, 0, 2, 0)
    tc.assertEqual((r["mode"], r["writes"], r["otdirect"]), (2, 0, 1), r)
    # forced PIC (1/3) never writes and never goes to OT-Direct
    for mode, pic in itertools.product((1, 3), (0, 1)):
        r = run(exe, 0, mode, pic)
        tc.assertEqual((r["mode"], r["writes"], r["otdirect"]), (mode, 0, 0), r)
    # 4. nothing ever persists 2 on its own (setup resolution and banner handler, all inputs)
    for where, mode, pic, pro in itertools.product((0, 1), (0, 1, 2, 3), (0, 1), (0, 1)):
        r = run(exe, where, mode, pic, pro)
        tc.assertEqual(r["saw2"], 0, (where, mode, pic, pro, r))
        tc.assertNotIn(2, r["values"], (where, mode, pic, pro, r))
        if mode == 0 and pic == 0 and where == 0:
            tc.assertEqual(r["writes"], 0, r)
    # banner safety net (OTGW-Core.ino): persists only a positive Classic verdict, only from auto
    for pro, want in ((0, 1), (1, 3)):
        r = run(exe, 1, 0, 1, pro)
        tc.assertEqual((r["mode"], r["values"], r["pic"]), (want, [want], 1), r)
    for mode in (1, 2, 3):
        r = run(exe, 1, mode, 1)
        tc.assertEqual((r["mode"], r["writes"]), (mode, 0), r)


class BoardDetectPersistGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(Path(cls.tmp.name), load(INO), load(CORE_INO), "head")
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler (g++, clang++ or MSVC) found")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_auto_pic_found_persists_classic(self):
        for pro, want in ((0, 1), (1, 3)):
            r = run(self.exe, 0, 0, 1, pro)
            self.assertEqual((r["mode"], r["values"], r["otdirect"]), (want, [want], 0), r)

    def test_auto_no_pic_runs_otdirect_without_persisting(self):
        r = run(self.exe, 0, 0, 0)
        self.assertEqual((r["mode"], r["writes"], r["otdirect"], r["detects"]), (0, 0, 1, 3), r)

    def test_forced_otdirect_with_pic_is_corrected(self):
        for pro, want in ((0, 1), (1, 3)):
            r = run(self.exe, 0, 2, 1, pro)
            self.assertEqual((r["mode"], r["values"]), (want, [want]), r)

    def test_nothing_persists_otdirect_on_its_own(self):
        for where, mode, pic, pro in itertools.product((0, 1), (0, 1, 2, 3), (0, 1), (0, 1)):
            r = run(self.exe, where, mode, pic, pro)
            self.assertEqual(r["saw2"], 0, (where, mode, pic, pro, r))

    def test_core_banner_persists_only_positive_verdict_from_auto(self):
        for pro, want in ((0, 1), (1, 3)):
            r = run(self.exe, 1, 0, 1, pro)
            self.assertEqual((r["mode"], r["values"]), (want, [want]), r)
        for mode in (1, 2, 3):
            r = run(self.exe, 1, mode, 1)
            self.assertEqual((r["mode"], r["writes"]), (mode, 0), r)

    def test_cut_blocks_still_have_their_shape(self):
        s = load(INO)
        self.assertEqual(s.count(NOPIC_LINE), 1)
        self.assertIn("if (settings.iBoardMode != 0) writeSettings(false);", s)


class MutantGate(unittest.TestCase):
    """The gate must go red on a no-PIC branch that persists OT-Direct (the ADR-164 behaviour)."""

    def test_mutant_is_caught(self):
        s = load(INO)
        self.assertEqual(s.count(NOPIC_LINE), 1, "mutation anchor moved; update the test")
        mutant = s.replace(NOPIC_LINE, NOPIC_LINE + "\n        settings.iBoardMode = 2;")
        with tempfile.TemporaryDirectory() as t:
            exe = build(Path(t), mutant, load(CORE_INO), "mutant")
            if exe is None:
                self.skipTest("no host C++ compiler found")
            r = run(exe, 0, 0, 0)
            self.assertEqual((r["mode"], r["saw2"], r["values"]), (2, 1, [2]), "mutant behaves as expected: persists 2")
            with self.assertRaises(AssertionError):
                check_contract(self, exe)


if __name__ == "__main__":
    unittest.main(verbosity=2)
