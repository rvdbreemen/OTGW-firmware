#!/usr/bin/env python3
"""TASK-1918: scripts/check_rtc_layout.py goes red when a main-firmware RTC_NOINIT record lands in safeboot's
0x50000200-0x5000021f, and green on the guarded layout.

The synthetic nm listings are the real beta.243+c6dc184 layout (old) and that layout shifted by the 32 B guard
(fix). When the archived beta.243 ELF and the toolchain nm are present, the real ELF must be red too.

Run: python tests/test_rtc_layout_check.py
"""
import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
import check_rtc_layout as chk  # noqa: E402

MARKERS = "50000200 A _rtc_noinit_start\n"

OLD = MARKERS + """50000200 00000034 B _ZL17g_rtcRebootReason$lto_priv$0
50000234 00000008 B _ZL21g_rtcWifiDeadRestarts$lto_priv$0
5000023c 00000008 B _ZL19g_rtcWifiSkipPortal$lto_priv$0
50000244 00001d10 B _ZL10g_fsOtaRtc$lto_priv$0
50001f54 A _rtc_noinit_end
50001f54 00000020 d s_sleep_sub_mode_ref_cnt
"""

FIX = MARKERS + """50000200 00000020 B g_rtcSafebootGuard
50000220 00000034 B _ZL17g_rtcRebootReason$lto_priv$0
50000254 00000008 B _ZL21g_rtcWifiDeadRestarts$lto_priv$0
5000025c 00000008 B _ZL19g_rtcWifiSkipPortal$lto_priv$0
50000264 00001d10 B _ZL10g_fsOtaRtc$lto_priv$0
50001f74 A _rtc_noinit_end
50001f74 00000020 d s_sleep_sub_mode_ref_cnt
"""

ARCHIVE_ELF = (REPO.parent / "OTGW-build-archive" / "beta.243-1930" /
               "OTGW-firmware-esp32-combo-2.0.0-beta.243+c6dc184.elf")


class RtcLayoutCheck(unittest.TestCase):
    def test_old_layout_is_red(self):
        bad = chk.check(OLD)
        self.assertTrue(any("g_rtcRebootReason" in b and "0x50000200" in b for b in bad), bad)

    def test_guarded_layout_is_green(self):
        self.assertEqual(chk.check(FIX), [])

    def test_missing_guard_is_red(self):
        no_guard = FIX.replace("50000200 00000020 B g_rtcSafebootGuard\n", "")
        self.assertTrue(any("not in .rtc_noinit" in b for b in chk.check(no_guard)))

    def test_record_moved_below_0x220_is_red(self):
        moved = FIX.replace("50000220 00000034 B _ZL17g_rtcRebootReason", "50000210 00000034 B _ZL17g_rtcRebootReason")
        self.assertTrue(any("g_rtcRebootReason" in b for b in chk.check(moved)))

    def test_guard_not_first_is_red(self):
        swapped = FIX.replace("50000200 00000020 B g_rtcSafebootGuard",
                              "50000300 00000020 B g_rtcSafebootGuard").replace(
            "50000220 00000034 B _ZL17g_rtcRebootReason", "50000200 00000034 B _ZL17g_rtcRebootReason")
        bad = chk.check(swapped)
        self.assertTrue(any("must start at 0x50000200" in b for b in bad), bad)
        self.assertTrue(any("g_rtcRebootReason" in b for b in bad), bad)

    def test_symbols_outside_rtc_noinit_are_ignored(self):
        # main's own .rtc.force_slow variable sits after _rtc_noinit_end and is not a record
        self.assertFalse(any("s_sleep_sub_mode_ref_cnt" in b for b in chk.check(FIX)))

    @unittest.skipUnless(ARCHIVE_ELF.exists() and chk.find_nm(), "archived beta.243 ELF or toolchain nm not present")
    def test_real_old_elf_is_red(self):
        r = subprocess.run([sys.executable, str(REPO / "scripts" / "check_rtc_layout.py"), str(ARCHIVE_ELF)],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn("g_rtcRebootReason", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
