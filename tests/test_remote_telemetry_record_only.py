#!/usr/bin/env python3
"""TASK-2901 (Robert 2026-11-20): full mode records every debug category but telnet shows only what the user
switched on ("alleen opnemen"); a debug line that repeats an event is not recorded twice.

Source checks (the gate is a GNU statement expression, so it cannot be compiled with the host MSVC):
  1. remoteTelemetryApplyRemote() sets the record mask and never writes state.debug.
  2. Every flag-gated family macro in the firmware goes through DEBUG_GATED (one call, telnet = the user's flag);
     only OTDebugFlush and SATTickTf keep their own form.
  3. The sinks: _debugBOLX and _debugVPrintf print on telnet only when telnet is true and always feed the recorder;
     RemoteTelemetryRecOnly never writes to debugTelnet.
  4. The "Sending to Serial" and "CmdQueue: Checking if command is in queue" lines are muted for the recorder (the
     EV_PIC_CMD and EV_RESPONSE events hold them).
Red on 4d3a410db, green on the change. RTEL_SRC=<dir> runs against another copy of src/OTGW-firmware.
Run: python tests/test_remote_telemetry_record_only.py
"""
import os
import re
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = Path(os.environ.get("RTEL_SRC", HERE.parent / "src" / "OTGW-firmware"))
FAMILY_FILES = ["MQTTstuff.ino", "restAPI.ino", "OTGW-Core.ino", "SATble.ino", "SATcontrol.ino", "SATcycles.ino",
                "OTDirect.ino"]


def text(name):
    return (SRC / name).read_text(encoding="utf-8")


def body(src, start):
    i = src.index(start)
    j = src.index("\n}\n", i) if "\n}\n" in src[i:] else src.index("\r\n}\r\n", i)
    return src[i:j]


class RecordOnly(unittest.TestCase):
    def test_apply_remote_never_writes_debug_flags(self):
        b = body(text("RemoteTelemetryDevice.h"), "static void remoteTelemetryApplyRemote()")
        self.assertNotIn("state.debug", b)
        self.assertIn("remoteTelemetryRecMask = want", b)

    def test_family_macros_use_the_gate(self):
        left = []
        for f in FAMILY_FILES:
            for n, line in enumerate(text(f).splitlines(), 1):
                if re.match(r"\s*#define\s+\w+\(", line) and "state.debug.b" in line:
                    name = re.match(r"\s*#define\s+(\w+)", line).group(1)
                    if name not in ("OTDebugFlush", "SATTickTf"):
                        left.append(f"{f}:{n} {name}")
        self.assertEqual(left, [], "family macros that bypass DEBUG_GATED")
        # SATTickTf keeps its telnet-or-recorder form (the SAT section, not the debug section, takes a tick)
        self.assertIn("#define SATTickTf(fmt, ...)   do { if (state.debug.bSAT) DebugTf(fmt, ##__VA_ARGS__); "
                      "else remoteTelemetrySatTickf(fmt, ##__VA_ARGS__); } while(0)", text("SATcontrol.ino"))
        for f in FAMILY_FILES:
            self.assertNotRegex(text(f), r"DEBUG_GATED\([^\n]*\);\s*else\b", f"{f}: a gate followed by else")
        self.assertIn("#define OTDebugTf(...)  DEBUG_GATED(bOTmsg, DBGREC_OTMSG, DebugTfX", text("OTGW-Core.ino"))

    def test_sinks(self):
        ino = text("debugStuff.ino")
        bol = body(ino, "void _debugBOLX(const char *fn, int line, bool telnet)")
        self.assertIn("if (telnet) debugTelnet.print(_bol);", bol)
        self.assertIn("remoteTelemetryBol(", bol)
        vp = body(ino, "static void _debugVPrintf(bool telnet, PGM_P fmt, va_list args)")
        self.assertIn("if (telnet) debugTelnet.print(buf);", vp)
        self.assertIn("remoteTelemetryPrintf(", vp)
        dev = text("RemoteTelemetryDevice.h")
        a = dev.index("size_t RemoteTelemetryRecOnly::write(uint8_t c)")
        b = dev.index("bool remoteTelemetryRecords") if "bool remoteTelemetryRecords" in dev[a:] else len(dev)
        rec = dev[a:dev.index("\n}\n", dev.index("RemoteTelemetryRecOnly::write(const uint8_t", a)) + 3]
        self.assertNotIn("debugTelnet", rec)
        h = text("debugStuff.h")
        self.assertRegex(h, r"#define DEBUG_GATED\(member, bit, sink, \.\.\.\) \\\s*\(\{ const bool _dbgT = state\.debug\.member; "
                            r"if \(_dbgT \|\| DEBUG_REC\(bit\)\) sink\(_dbgT, ##__VA_ARGS__\); \}\)")

    def test_event_duplicates_muted(self):
        core = text("OTGW-Core.ino")
        for marker in ('OTDebugT(F("Sending to Serial ["));', 'OTDebugT(F("CmdQueue: Checking if command is in queue ["));'):
            i = core.index(marker)
            self.assertIn("remoteTelemetryMute(true);", core[max(0, i - 200):i], marker)
            self.assertIn("remoteTelemetryMute(false);", core[i:i + 400], marker)


if __name__ == "__main__":
    unittest.main(verbosity=2)
