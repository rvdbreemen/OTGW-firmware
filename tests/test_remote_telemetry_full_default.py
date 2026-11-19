#!/usr/bin/env python3
"""TASK-2901 (Robert 2026-11-20): "default is full mode", "ALLE SAT beslissingen", "zorg dat de full stream geen
dubbele informatie bevat".

1. Compiles the real rtel::remoteEffective() of RemoteTelemetryPolicy.h (only the mode part: the parser uses GCC
   builtins) and checks: default, an empty mode, full and full+trace switch on all ten log flags, sattrace included;
   custom switches on its flags only; any mode past mode_until falls back to default, i.e. all ten.
2. Checks in the source that nothing reaches the stream twice:
   - the decoded OT line per frame (OTGW-Core.ino, OTDebugT(skipOTLogTimestamp(...))) is muted for the recorder,
     the frame itself is in the OT section;
   - a SATTICK line that went into the SAT section returns before the debug-token path;
   - the telemetry task's own per-upload line is muted;
   - the tee, the line prefix and the printf capture all honour the mute.
Red on b11d02e9e (default = no flags, full = no sattrace, no mute, SATTICK in both sections).
RTEL_SRC=<dir> runs the checks against another copy of src/OTGW-firmware.
Run: python tests/test_remote_telemetry_full_default.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = Path(os.environ.get("RTEL_SRC", HERE.parent / "src" / "OTGW-firmware"))


def text(name):
    return (SRC / name).read_text(encoding="utf-8")


MAIN = r"""
#include <cstdio>
int main() {
  using namespace rtel;
  RemoteCfg cs[] = {{RM_DEFAULT, 0, 0}, {RM_FULL, 0, 0}, {RM_FULL_TRACE, 0, 0}, {RM_CUSTOM, (uint16_t)(1u << 4), 0},
                    {RM_CUSTOM, (uint16_t)(1u << 4), 1000}, {RM_FULL, 0, 1000}};
  for (auto &c : cs) printf("%u %u\n", (unsigned)remoteEffective(c, 999), (unsigned)remoteEffective(c, 1000));
  return 0;
}
"""


class FullIsDefault(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pol = text("RemoteTelemetryPolicy.h")
        a = pol.index("static const char *const REMOTE_FLAGS")
        b = pol.index("// Strict ", a) if "// Strict " in pol[a:] else None
        cfg_end = pol.index("};", pol.index("struct RemoteCfg", a)) + 2
        e = pol.index("inline uint16_t remoteEffective")
        e_end = pol.index("\n}\n", e) + 3
        body = "namespace rtel {\n" + pol[a:cfg_end] + "\n" + pol[e:e_end] + "}\n"
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "m.cpp"
        cpp.write_text("#include <cstddef>\n#include <cstdint>\n" + body + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / ("m.exe" if os.name == "nt" else "m"))
        if exe is None:
            raise unittest.SkipTest("no host compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.split()
        cls.v = [(int(out[i]), int(out[i + 1])) for i in range(0, len(out), 2)]

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    ALL = (1 << 10) - 1
    SAT = 1 << 4

    def test_default_full_and_trace_are_all_ten_flags(self):
        for i in (0, 1, 2):
            self.assertEqual(self.v[i][0], self.ALL, i)
            self.assertEqual(self.v[i][1], self.ALL, i)

    def test_custom_is_its_flags_and_expires_to_default(self):
        self.assertEqual(self.v[3], (self.SAT, self.SAT))      # no mode_until: holds
        self.assertEqual(self.v[4], (self.SAT, self.ALL))      # past mode_until: default = full
        self.assertEqual(self.v[5], (self.ALL, self.ALL))


class NothingTwice(unittest.TestCase):
    def test_ot_decode_line_muted(self):
        core = text("OTGW-Core.ino")
        m = re.search(r"remoteTelemetryMute\(true\);[^\n]*\n(?:#endif\n)?\s*OTDebugT\(skipOTLogTimestamp\(ot_log_buffer\)\);"
                      r"\s*\n(?:#if HAS_REMOTE_TELEMETRY\n)?\s*remoteTelemetryMute\(false\);", core)
        self.assertIsNotNone(m, "the decoded OT line per frame must be muted for the recorder")

    def test_capture_points_honour_the_mute(self):
        dev = text("RemoteTelemetryDevice.h")
        # the tee and the record-only print share one byte capture (TASK-2901 record-only)
        self.assertRegex(dev, r"static void remoteTelemetryTeeByte\(uint8_t c\) \{\s*"
                              r"if \(remoteTelemetryActive && !remoteTelemetryMuted\(\)\)")
        self.assertRegex(dev, r"size_t RemoteTelemetryTee::write\(uint8_t c\) \{\s*debugTelnet\.write\(c\);\s*"
                              r"remoteTelemetryTeeByte\(c\);")
        self.assertRegex(dev, r"void remoteTelemetryBol\([^)]*\) \{\s*if \(!remoteTelemetryActive \|\| remoteTelemetryMuted\(\)\) return;")
        self.assertRegex(dev, r"void remoteTelemetryPrintf\([^)]*\) \{\s*if \(!remoteTelemetryActive \|\| remoteTelemetryMuted\(\)\) return;")

    def test_sattick_not_also_a_debug_line(self):
        dev = text("RemoteTelemetryDevice.h")
        a = dev.index("void remoteTelemetryPrintf(")
        b = dev.index("if (recBolPending && remoteTelemetryInFlash", a)
        tick = dev[dev.index('"SATTICK "', a):b]
        self.assertIn("return;", tick, "a recorded SATTICK must not fall through to the debug-token path")

    def test_upload_line_muted(self):
        dev = text("RemoteTelemetryDevice.h")
        self.assertRegex(dev, r"remoteTelemetryMute\(true\);\s*DebugTf\(PSTR\(\"telemetry: seq[\s\S]{0,700}?"
                              r"remoteTelemetryMute\(false\);")


if __name__ == "__main__":
    unittest.main(verbosity=2)
