#!/usr/bin/env python3
"""TASK-1508 (ADR-209): host test for the replay timing of /otgw_simulation.log.

The real code is cut out of src/OTGW-firmware/OTGW-Core.ino (from dispatchOTGWInputLine() up to handlePICSerialSimulation(): the line reader, the otframes.log
parser and the scheduler in replayNextOTGWSimulationLine()) and compiled with a host C++
compiler; nothing is re-implemented. millis() is a stub clock stepped 1 ms at a time, File is an
in-memory file, dispatchOTGWInputLine() records (time, line). The driver mirrors the pump
condition of handlePICSerialSimulation() (next-due check). OTGW_CORE_INO points at another copy
(for example `git show HEAD:src/OTGW-firmware/OTGW-Core.ino`) for old-vs-fix runs. Without a
compiler the tests are skipped.

Old behaviour: every line is dispatched 750 ms after the previous one, whatever the file says,
and a tab separated line is dispatched as it is.

Run: python tests/test_replay_timing.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

CORE_INO = Path(os.environ.get("OTGW_CORE_INO", REPO / "src" / "OTGW-firmware" / "OTGW-Core.ino"))
START = "// dispatchOTGWInputLine"
END = "static bool handlePICSerialSimulation("

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#define PSTR(x) x
#define F(x) x
#define DebugTln(...)
#define HAS_PIC 0
#define pgm_read_byte(p) (*(const unsigned char*)(p))
#define PROGMEM
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static void feedWatchDog() {}
static bool LittleFSmounted = true;
static std::string g_data;       // the "LittleFS file"; empty = no file
struct File {
  size_t pos = 0; bool open = false;
  explicit operator bool() const { return open; }
  bool available() { return open && pos < g_data.size(); }
  int read() { return (unsigned char)g_data[pos++]; }
  void close() { open = false; pos = 0; }
};
struct FS {
  bool exists(const char*) { return !g_data.empty(); }
  File open(const char*, const char*) { File f; f.open = true; return f; }
};
static FS LittleFS;
struct MqttS { bool bLegacyPort25238Enabled = false; };
struct Settings { MqttS mqtt; };
static Settings settings;
struct DebugS { uint32_t iOTGWSimulationIntervalMs = 750; uint32_t iOTGWSimulationNextDueMs = 0; };
struct State { DebugS debug; };
static State state;
struct Stream { void write(const uint8_t*, size_t) {} void write(char) {} };
static Stream OTGWstream;
#define OTFRAME_SRC_REPLAY 1
struct Rec { uint32_t t; std::string line; };
static std::vector<Rec> g_rec;
static void enqueueOTFrame(const char* b, size_t n, bool, int) { g_rec.push_back({g_ms, std::string(b, n)}); }
@BODY@
int main(int argc, char** argv) {
  // argv[1]: data file ("-" = none, built-in fixture), argv[2]: number of lines to collect
  if (strcmp(argv[1], "-")) {
    FILE* f = fopen(argv[1], "rb"); char c;
    while (f && fread(&c, 1, 1, f) == 1) g_data.push_back(c);
    if (f) fclose(f);
  }
  const size_t want = (size_t)atoi(argv[2]);
  File file; static char buf[512];
  for (g_ms = 1000; g_rec.size() < want && g_ms < 1000 + 600000; g_ms++) {
    const uint32_t due = state.debug.iOTGWSimulationNextDueMs;   // the pump condition
    if (due == 0 || (int32_t)(g_ms - due) >= 0) replayNextOTGWSimulationLine(file, buf, sizeof(buf));
  }
  for (auto& r : g_rec) printf("%u\t%s\n", (unsigned)(r.t - 1000), r.line.c_str());
#ifdef HAVE_MODE
  printf("mode=%s\n", otgwReplayTimingMode());
#endif
  return 0;
}
"""


def build(workdir: Path):
    text = CORE_INO.read_text(encoding="utf-8")
    a, b = text.index(START), text.index(END)
    body = text[a:b].replace('#include "otgwSimulationFixture.h"', "")
    fixture = (REPO / "src" / "OTGW-firmware" / "otgwSimulationFixture.h").read_text(encoding="utf-8")
    fixture = fixture.replace("#pragma once", "")
    cpp = workdir / "replay.cpp"
    defs = "#define HAVE_MODE 1\n" if "otgwReplayTimingMode" in body else ""
    cpp.write_text(defs + STUBS.replace("@BODY@", fixture + "\n" + body), encoding="utf-8")
    return compile_cpp(cpp, workdir / "replay.exe")


def frames_line(dev, letter, frame):
    return f"2026-11-04T10:00:00.000+01:00\t{dev}\t{letter}\t{frame}\n"


class ReplayTiming(unittest.TestCase):
    exe = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        try:
            cls.exe = build(Path(cls.tmp.name))
        except subprocess.CalledProcessError as exc:
            raise AssertionError(f"host compile failed: {exc}")
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_replay(self, content: str, want: int):
        path = Path(self.tmp.name) / "sim.log"
        if content is None:
            arg = "-"
        else:
            path.write_bytes(content.encode("ascii"))
            arg = str(path)
        out = subprocess.run([str(self.exe), arg, str(want)], capture_output=True, text=True, check=True).stdout
        rows, mode = [], None
        for line in out.splitlines():
            if line.startswith("mode="):
                mode = line[5:]
                continue
            t, text = line.split("\t", 1)
            rows.append((int(t), text))
        return rows, mode

    def test_mixed_gaps_follow_the_recording(self):
        # device times: gaps 0.2 s, 1.0 s, 3.5 s, 0.9995 s
        times = ["10:00:00.000000", "10:00:00.200000", "10:00:01.200000", "10:00:04.700000", "10:00:05.699500"]
        frames = ["T00390000", "BC0393500", "T08000800", "B40000800", "T10010000"]
        content = "# note line\n" + "".join(frames_line(t, f[0], f[1:]) for t, f in zip(times, frames))
        rows, mode = self.run_replay(content, 5)
        self.assertEqual([r[1] for r in rows], frames)           # the line reaches the parser as a bare frame
        want = [200, 1000, 3500, 999]                            # 0.9995 s truncates to 999 ms
        got = [rows[i + 1][0] - rows[i][0] for i in range(4)]
        for g, w in zip(got, want):
            self.assertLessEqual(abs(g - w), 50, f"gaps {got} vs recorded {want}")
        if mode is not None:
            self.assertEqual(mode, "timed")

    def test_three_column_and_midnight_and_clamp(self):
        content = (
            "23:59:59.500000\tT\t00390000\n"
            "00:00:00.500000\tB\tC0393500\n"      # across midnight: 1.0 s
            "00:02:00.500000\tT\t08000800\n"      # 120 s: clamped to 60 s
        )
        rows, _ = self.run_replay(content, 3)
        self.assertEqual([r[1] for r in rows], ["T00390000", "BC0393500", "T08000800"])
        self.assertLessEqual(abs((rows[1][0] - rows[0][0]) - 1000), 50)
        self.assertLessEqual(abs((rows[2][0] - rows[1][0]) - 60000), 50)

    def test_bare_frames_keep_750_ms(self):
        rows, mode = self.run_replay("T00390000\nBC0393500\nT08000800\nB40000800\n", 4)
        self.assertEqual([r[1] for r in rows], ["T00390000", "BC0393500", "T08000800", "B40000800"])
        for i in range(3):
            self.assertEqual(rows[i + 1][0] - rows[i][0], 750)
        if mode is not None:
            self.assertEqual(mode, "fixed")

    def test_builtin_fixture_keeps_750_ms(self):
        rows, mode = self.run_replay(None, 4)
        for i in range(3):
            self.assertEqual(rows[i + 1][0] - rows[i][0], 750)
        if mode is not None:
            self.assertEqual(mode, "fixed")


if __name__ == "__main__":
    unittest.main(verbosity=2)
