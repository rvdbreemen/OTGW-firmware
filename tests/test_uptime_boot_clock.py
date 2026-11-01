#!/usr/bin/env python3
"""TASK-1449 (port of 1.x TASK-1289): the uptime is derived from the 64-bit boot clock.

state.uptime.iSeconds was incremented once per 1 s timer tick in doTaskEvery1s() (SKIP_MISSED_TICKS), so it
missed all of setup() (a blocking WiFi portal can last minutes) and every loop stall above 1 s. It is now
assigned from platformUptimeSeconds() = esp_timer_get_time() / 1e6 (int64 microseconds since boot), not from
micros() (32-bit, wraps after ~71.6 minutes). The 1.x loopSeconds gate is not ported: sendPICBootCommands()
stays in setup().

Old-vs-fix: OTGW_INO / OTGW_PLATFORM_H point at other copies.

Run: python tests/test_uptime_boot_clock.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
INO = Path(os.environ.get("OTGW_INO", REPO / "src" / "OTGW-firmware" / "OTGW-firmware.ino"))
PLAT = Path(os.environ.get("OTGW_PLATFORM_H", REPO / "src" / "libraries" / "Platform" / "src" / "platform_esp32.h"))


class UptimeBootClock(unittest.TestCase):
    def setUp(self):
        self.ino = INO.read_text(encoding="utf-8")
        self.plat = PLAT.read_text(encoding="utf-8")

    def test_shim_uses_the_64_bit_boot_clock(self):
        m = re.search(r"inline uint32_t platformUptimeSeconds\(\) \{(.*?)\}", self.plat, re.S)
        self.assertIsNotNone(m, "platformUptimeSeconds() missing")
        self.assertIn("esp_timer_get_time()", m.group(1))
        self.assertNotIn("micros()", m.group(1))

    def test_uptime_is_assigned_not_counted(self):
        body = self.ino[self.ino.index("void doTaskEvery1s()"):]
        body = body[:body.index("\n}\n") if "\n}\n" in body else len(body)]
        self.assertNotIn("state.uptime.iSeconds++", body)
        self.assertIn("state.uptime.iSeconds = platformUptimeSeconds();", body)

    def test_no_loop_seconds_gate_ported(self):
        self.assertNotIn("loopSeconds", self.ino)
        self.assertIn("sendPICBootCommands()", self.ino)


if __name__ == "__main__":
    unittest.main(verbosity=2)
