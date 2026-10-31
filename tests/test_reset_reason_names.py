#!/usr/bin/env python3
"""TASK-1438: every ESP-IDF reset reason has a name in the reboot log.

The Classic+S3 bench logged 'reboot cause: Unknown (b)'. The code is esp_reset_reason() (platformResetCode),
and 0xb = 11 = ESP_RST_USB in ESP-IDF esp_system.h (the order of esp_reset_reason_t). platformResetReason()
named only the values up to ESP_RST_SDIO (10), so 11-15 fell into "Unknown".

The test reads the esp_reset_reason_t enum from the installed framework header when it is present and checks
that platformResetReason() has a case for every member except ESP_RST_UNKNOWN.

Old-vs-fix: OTGW_PLATFORM_H points at another copy of platform_esp32.h.

Run: python tests/test_reset_reason_names.py
"""
import glob
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
PLAT = Path(os.environ.get("OTGW_PLATFORM_H", REPO / "src" / "libraries" / "Platform" / "src" / "platform_esp32.h"))
KNOWN = ["ESP_RST_POWERON", "ESP_RST_EXT", "ESP_RST_SW", "ESP_RST_PANIC", "ESP_RST_INT_WDT", "ESP_RST_TASK_WDT",
         "ESP_RST_WDT", "ESP_RST_DEEPSLEEP", "ESP_RST_BROWNOUT", "ESP_RST_SDIO", "ESP_RST_USB", "ESP_RST_JTAG",
         "ESP_RST_EFUSE", "ESP_RST_PWR_GLITCH", "ESP_RST_CPU_LOCKUP"]


def idf_reasons():
    roots = [os.environ.get("PLATFORMIO_CORE_DIR", ""), r"D:\DevData\platformio", str(Path.home() / ".platformio")]
    for root in filter(None, roots):
        for h in glob.glob(os.path.join(root, "packages", "framework-arduinoespressif32-libs", "esp32s3", "include",
                                        "esp_system", "include", "esp_system.h")) + \
                 glob.glob(os.path.join(root, "packages", "framework-arduinoespressif32-libs", "esp32", "include",
                                        "esp_system", "include", "esp_system.h")):
            text = Path(h).read_text(encoding="utf-8", errors="replace")
            m = re.search(r"typedef enum \{(.*?)\} esp_reset_reason_t;", text, re.S)
            if m:
                return re.findall(r"^\s*(ESP_RST_\w+)", m.group(1), re.M), h
    return None, None


class ResetReasonNames(unittest.TestCase):
    def setUp(self):
        src = PLAT.read_text(encoding="utf-8")
        i = src.index("inline void platformResetReason(")
        self.body = src[i:src.index("strlcpy(buf, str, len);", i)]

    def test_usb_0xb_is_named(self):
        self.assertRegex(self.body, r"case ESP_RST_USB:\s*str = \"USB reset")

    def test_every_known_reason_has_a_case(self):
        for r in KNOWN:
            with self.subTest(reason=r):
                self.assertIn("case " + r + ":", self.body)

    def test_idf_header_order_puts_usb_at_11(self):
        reasons, path = idf_reasons()
        if not reasons:
            self.skipTest("ESP-IDF esp_system.h not found")
        self.assertEqual(reasons.index("ESP_RST_USB"), 11, path)
        for r in reasons[1:]:
            with self.subTest(reason=r):
                self.assertIn("case " + r + ":", self.body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
