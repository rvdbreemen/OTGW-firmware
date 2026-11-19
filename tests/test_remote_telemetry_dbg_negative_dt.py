#!/usr/bin/env python3
"""TASK-2901: a debug item a few microseconds earlier than the previous one decodes (no OSError, no abort).

poc.22 export (2026-11-19): 2 of 463 records' debug sections failed with OSError in fromtimestamp. The trace showed a
time delta of 18446744073709551324 = 2**64 - 292: two tasks logged almost at once and the later item carried a time
292 us before the previous one; the device sent the wrapped unsigned 64-bit difference. tools/remote-telemetry/otrec.py
dbg_decode() now reads a delta >= 2**63 as negative. Red on 1b31d84ff (OSError), green on the fix.
Run: python tests/test_remote_telemetry_dbg_negative_dt.py
"""
import sys
import unittest
from pathlib import Path
from zoneinfo import ZoneInfo

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools" / "remote-telemetry"))
import otrec  # noqa: E402


class FakeImage:
    def read_cstr(self, addr):
        return "fnName"


def leb(n):
    return otrec._leb(n)


class NegativeDelta(unittest.TestCase):
    def test_wrapped_negative_delta_goes_back_in_time(self):
        t0_ms = 1795120617000                                   # 2026-11-19 21:36:57 local
        line = b"Sending to Serial [PR=R] (4)\r\n"
        item = lambda dt: bytes([otrec.DBG_LINE_RAW]) + leb(dt) + (0x123456).to_bytes(3, "little") + leb(4587) + leb(len(line)) + line
        data = item(125024) + item((1 << 64) - 292)            # +125024 us, then 292 us back
        out = otrec.dbg_decode(t0_ms, data, FakeImage(), ZoneInfo("Europe/Amsterdam")).decode()
        stamps = [l.split(" ", 1)[0] for l in out.splitlines() if l.strip()]
        self.assertEqual(len(stamps), 2, out)
        self.assertEqual(stamps[0], "21:36:57.125024", out)
        self.assertEqual(stamps[1], "21:36:57.124732", out)   # 292 us earlier, as the device logged it


if __name__ == "__main__":
    unittest.main(verbosity=2)
