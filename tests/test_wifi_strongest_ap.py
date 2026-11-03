#!/usr/bin/env python3
"""
TASK-1397: every WiFi (re)connect must pick the strongest AP of the SSID.

Arduino-ESP32 3.3.5 defaults to WIFI_FAST_SCAN (joins the first AP found). The
shim platformWifiPreferStrongestAp() sets WIFI_ALL_CHANNEL_SCAN +
WIFI_CONNECT_AP_BY_SIGNAL and must run before every WiFi.begin() in the sources.

Source-level test on the real files (it cannot run the radio; the two-AP check
is a bench test).

Run: python tests/test_wifi_strongest_ap.py   (OTGW_TEST_ROOT overrides the tree)
"""

import os
import re
import unittest
from pathlib import Path

ROOT = Path(os.environ.get("OTGW_TEST_ROOT", Path(__file__).resolve().parent.parent))
FW = ROOT / "src" / "OTGW-firmware"
PLATFORM = ROOT / "src" / "libraries" / "Platform" / "src" / "platform_esp32.h"
SHIM = "platformWifiPreferStrongestAp"
LOOKBACK = 6  # lines


def strip_comment(line):
    return line.split("//", 1)[0]


class WifiStrongestApTest(unittest.TestCase):
    def test_shim_sets_both_methods(self):
        text = PLATFORM.read_text(encoding="utf-8", errors="replace")
        m = re.search(r"inline void " + SHIM + r"\(\)\s*\{(.*?)\n\}", text, re.S)
        self.assertIsNotNone(m, SHIM + " missing in platform_esp32.h")
        body = m.group(1)
        self.assertIn("WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN)", body)
        self.assertIn("WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL)", body)
        # bare begin() and SDK auto-reconnect reuse the stored driver config
        self.assertIn("esp_wifi_set_config", body)
        self.assertIn("cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN", body)
        self.assertIn("cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL", body)

    def test_shim_before_every_wifi_begin(self):
        begins = 0
        bad = []
        for path in sorted(FW.glob("*.ino")) + sorted(FW.glob("*.cpp")):
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            for i, line in enumerate(lines):
                if re.search(r"\bWiFi\.begin\s*\(", strip_comment(line)):
                    begins += 1
                    window = "\n".join(strip_comment(l) for l in lines[max(0, i - LOOKBACK):i])
                    if SHIM not in window:
                        bad.append("%s:%d" % (path.name, i + 1))
        self.assertGreaterEqual(begins, 4, "expected the known WiFi.begin() sites")
        self.assertEqual(bad, [], "WiFi.begin() without %s before it" % SHIM)

    def test_startwifi_sets_it_before_wifimanager(self):
        text = (FW / "networkStuff.ino").read_text(encoding="utf-8", errors="replace")
        start = text.index("void startWiFi(")
        pos_shim = text.find(SHIM, start)
        pos_wm = text.find("WiFiManager manageWiFi;", start)
        self.assertNotEqual(pos_shim, -1)
        self.assertLess(pos_shim, pos_wm)


if __name__ == "__main__":
    unittest.main()
