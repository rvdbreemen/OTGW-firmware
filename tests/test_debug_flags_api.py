#!/usr/bin/env python3
"""TASK-1561: the telnet debug log flags can be read and set explicitly over REST.

Before: every debug flag was a telnet key that toggles, so a script had to guess the state (otgw_capture.py
parse_toggles) or it turned a flag off by accident. Now src/OTGW-firmware/debugFlags.h holds one table
(name, telnet key, DebugSection member, label) that the telnet keys and GET/POST /api/v2/debug/flags share.

What runs on the host:
- the real debugFlags.h, compiled with the real DebugSection struct cut out of debugStuff.h: set by name,
  idempotent, unknown name and non-bool value rejected, names and keys unique;
- a drift guard over handleDebug.ino: every remaining "state.debug.bX = !state.debug.bX" toggle is one of the
  behaviour switches kept out of the table, and every bool in DebugSection is either in the table or on that list;
- a structural check of restAPI.ino: /api/v2/debug/flags goes through checkHttpAuth() and the shared table.
  The REST handler itself (AsyncWebServer) is not run here; the bench covers it.

OTGW_FW_DIR points at another copy of src/OTGW-firmware for old-vs-fix runs.
Run: python tests/test_debug_flags_api.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
FW = Path(os.environ.get("OTGW_FW_DIR", REPO / "src" / "OTGW-firmware"))
EXCLUDED = {"bSensorSim", "bOTGWSimulation"}   # behaviour switches with side effects: telnet keys only

MAIN = r"""
#include <cstdio>
int main() {
  DebugSection d;
  int err = -9;
  d.bSAT = true;
  const DebugFlagDef* f = debugFlagSet(d, "sat", "false", &err);
  printf("off1|%d|%d|%s\n", err, (int)d.bSAT, f ? f->label : "-");
  f = debugFlagSet(d, "sat", "false", &err);
  printf("off2|%d|%d\n", err, (int)d.bSAT);
  f = debugFlagSet(d, "SAT", "true", &err);
  printf("on1|%d|%d\n", err, (int)d.bSAT);
  f = debugFlagSet(d, "sat", "on", &err);
  printf("on2|%d|%d\n", err, (int)d.bSAT);
  f = debugFlagSet(d, "nosuchflag", "true", &err);
  printf("unknown|%d|%d\n", err, f ? 1 : 0);
  f = debugFlagSet(d, "sat", "maybe", &err);
  printf("badval|%d|%d|%d\n", err, f ? 1 : 0, (int)d.bSAT);
  f = debugFlagSet(d, "mqtt", "1", &err);
  printf("mqtt|%d|%d\n", err, (int)d.bMQTT);
  for (size_t i = 0; i < kDebugFlagCount; i++) printf("row|%s|%c|%s\n", kDebugFlags[i].name, kDebugFlags[i].key, kDebugFlags[i].label);
  return 0;
}
"""


class DebugFlagsApi(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = FW / "debugFlags.h"
        if not header.exists():
            raise AssertionError(f"{header} missing: no shared debug-flag table (TASK-1561 not implemented)")
        dbg = (FW / "debugStuff.h").read_text(encoding="utf-8")
        m = re.search(r"struct DebugSection \{.*?\n\};", dbg, re.S)
        cls.struct = m.group(0)
        cls.bools = set(re.findall(r"^\s*bool\s+(b\w+)", cls.struct, re.M))
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text("#include <cstdint>\n#include <cstddef>\n#define HAS_SAT_BLE 1\n" + cls.struct + "\n"
                       + header.read_text(encoding="utf-8") + "\n" + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.rows, cls.table = {}, []
        for line in out.splitlines():
            p = line.split("|")
            if p[0] == "row":
                cls.table.append(tuple(p[1:]))
            else:
                cls.rows[p[0]] = p[1:]
        cls.header_src = header.read_text(encoding="utf-8")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_set_is_explicit_and_idempotent(self):
        self.assertEqual(self.rows["off1"], ["0", "0", "SAT"])
        self.assertEqual(self.rows["off2"], ["0", "0"])     # the same request twice leaves it off (a toggle would flip it)
        self.assertEqual(self.rows["on1"], ["0", "1"])      # name is case-insensitive
        self.assertEqual(self.rows["on2"], ["0", "1"])
        self.assertEqual(self.rows["mqtt"], ["0", "1"])

    def test_unknown_name_and_bad_value_are_rejected_without_a_change(self):
        self.assertEqual(self.rows["unknown"], ["1", "0"])
        self.assertEqual(self.rows["badval"], ["2", "0", "1"])

    def test_names_and_keys_are_unique(self):
        names = [r[0] for r in self.table]
        keys = [r[1] for r in self.table]
        self.assertEqual(len(names), len(set(names)), names)
        self.assertEqual(len(keys), len(set(keys)), keys)

    def test_every_debug_bool_is_in_the_table_or_excluded(self):
        members = set(re.findall(r"&DebugSection::(b\w+)", self.header_src))
        self.assertEqual(self.bools - members - EXCLUDED, set(), "DebugSection flag without a REST name")
        self.assertEqual(members & EXCLUDED, set(), "a behaviour switch must not be in the log-flag table")

    def test_telnet_keys_use_the_table(self):
        src = (FW / "handleDebug.ino").read_text(encoding="utf-8")
        toggles = set(re.findall(r"state\.debug\.(b\w+)\s*=\s*!state\.debug\.\1", src))
        self.assertEqual(toggles - EXCLUDED, set(), f"telnet toggles outside the shared table: {toggles - EXCLUDED}")
        self.assertIn("debugFlagByKey(", src)

    def test_rest_route_is_authenticated_and_uses_the_table(self):
        src = (FW / "restAPI.ino").read_text(encoding="utf-8")
        a = src.find("static void handleDebugFlags(")
        self.assertGreater(a, 0, "no handleDebugFlags() in restAPI.ino")
        b = src.find("\n}\n", a)
        body = src[a:b]
        self.assertIn("checkHttpAuth()", body)
        self.assertIn("debugFlagSet(", body)
        self.assertIn("kDebugFlags", body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
