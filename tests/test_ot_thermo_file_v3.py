#!/usr/bin/env python3
"""TASK-1895 (Robert's choice A, DM 1570749159060013087): /ot-thermo.json goes to format 3.

Format 2 was written by builds that also counted replayed T lines (OTGW simulation) as thermostat requests, so a unit that
ever ran a replay may hold false thermostat bits. A format-2 file is dropped once at boot (the real thermostat relearns its
bits from live frames) and the next save writes format 3. /ot-boiler.json is not affected.

REAL code, cut from src/OTGW-firmware/OTGW-Core.ino and compiled on the host: loadOtSupportFiles(). STUBBED: an in-memory
LittleFS with File::read/close, readBitmapArrayFromJson (it records which arrays were read), PSTR/strstr_P/F, DebugTln.
The writer's format byte is pinned by regex on the real writeOtThermoFile().
Old-vs-fix: OTGW_CORE_INO=<older OTGW-Core.ino> python tests/test_ot_thermo_file_v3.py
Run: python tests/test_ot_thermo_file_v3.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402

CORE = Path(os.environ.get("OTGW_CORE_INO", REPO / "src" / "OTGW-firmware" / "OTGW-Core.ino"))

PRELUDE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#define PSTR(x) x
#define F(x) x
#define strstr_P strstr
#define DebugTln(...) do {} while (0)
typedef char __FlashStringHelper;
static std::map<std::string, std::string> g_fs;
struct File {
  std::string data; size_t pos = 0; bool ok = false;
  explicit operator bool() const { return ok; }
  int read(uint8_t* b, size_t n) { size_t k = std::min(n, data.size() - pos); memcpy(b, data.data() + pos, k); pos += k; return (int)k; }
  void close() {}
};
struct FS { File open(const char* p, const char*) { File f; auto it = g_fs.find(p); if (it != g_fs.end()) { f.data = it->second; f.ok = true; } return f; } };
static FS LittleFS;
static std::string g_read;   // which arrays were read, in order
static void readBitmapArrayFromJson(File&, const char* key, uint8_t*) { g_read += key; g_read += ' '; }
static uint8_t thermostatSentRead[32], thermostatSentWrite[32];
static uint8_t boilerAckedRead[32], boilerAckedWrite[32], boilerUnsupportedRead[32], boilerUnsupportedWrite[32];
static bool thermostatFileDirty, boilerFileDirty;
"""

MAIN = r"""
int main(int argc, char** argv) {
  if (argv[1][0] != '-') g_fs["/ot-thermo.json"] = std::string("{\"v\":") + argv[1] + ",\"device\":\"thermostat\",\"sr\":[17],\"sw\":[14]}";
  if (argv[2][0] != '-') g_fs["/ot-boiler.json"] = std::string("{\"v\":") + argv[2] + ",\"device\":\"boiler\",\"ar\":[0]}";
  loadOtSupportFiles();
  printf("read=%s|tdirty=%d bdirty=%d\n", g_read.c_str(), thermostatFileDirty ? 1 : 0, boilerFileDirty ? 1 : 0);
  return 0;
}
"""


def cut_fn(src, head):
    i = src.index(head)
    j = src.index("{", i)
    depth = 0
    for k in range(j, len(src)):
        depth += {"{": 1, "}": -1}.get(src[k], 0)
        if depth == 0:
            return src[i:k + 1]
    raise AssertionError("unbalanced braces after " + head)


class ThermoFileV3(unittest.TestCase):
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.src = CORE.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "t.cpp"
        cpp.write_text(PRELUDE + cut_fn(cls.src, "void loadOtSupportFiles()") + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("t.exe" if os.name == "nt" else "t"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def load(self, thermo, boiler):
        out = subprocess.run([str(self.exe), thermo, boiler], capture_output=True, text=True, check=True).stdout.strip()
        read, rest = out.split("|")
        kv = dict(x.split("=") for x in rest.split())
        return read[len("read="):].split(), kv["tdirty"] == "1", kv["bdirty"] == "1"

    def test_v2_thermostat_file_is_dropped_once_and_rewritten(self):
        read, tdirty, _ = self.load("2", "3")
        self.assertNotIn('"sr":[', read, "a format-2 /ot-thermo.json was loaded")
        self.assertTrue(tdirty, "the dropped file must be rewritten in the current format")

    def test_v3_thermostat_file_is_kept(self):
        read, tdirty, _ = self.load("3", "3")
        self.assertEqual(read[:2], ['"sr":[', '"sw":['])
        self.assertFalse(tdirty)

    def test_v1_thermostat_file_is_still_dropped(self):
        read, tdirty, _ = self.load("1", "-")
        self.assertEqual(read, [])
        self.assertTrue(tdirty)

    def test_boiler_file_untouched(self):
        for thermo in ("2", "3", "-"):
            with self.subTest(thermo=thermo):
                read, _, bdirty = self.load(thermo, "3")
                self.assertEqual(read[-4:], ['"ar":[', '"aw":[', '"ur":[', '"uw":['])
                self.assertFalse(bdirty, "/ot-boiler.json format 3 must stay as it is")

    def test_writer_writes_format_3(self):
        w = cut_fn(self.src, "static bool writeOtThermoFile(")
        self.assertRegex(w, r'f\.print\(F\("\{\\"v\\":3,\\"device\\":\\"thermostat\\"')


if __name__ == "__main__":
    unittest.main(verbosity=2)
