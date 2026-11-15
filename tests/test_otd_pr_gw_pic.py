#!/usr/bin/env python3
"""TASK-1804 B3c and B4: PR= checks and the GW=R banner like the PIC (source checks on the real branches).

PIC ground truth (other-projects/otgw-6.6/gateway.asm):
- PrintSetting :2150-2186: the command must be exactly "PR=X" (else SE); a letter outside A-Z, or one the PrintTable has no
  report for (H, J, K, U, X, Y, Z), is BV. PrintSettingE prints '-' without an external sensor (OT-Direct has none).
  PR=F reads the PIC's configuration word: no OT-Direct counterpart, answered BV (a recorded difference).
- ResetGateway resets; at boot the PIC greets with GreetingStr "OpenTherm Gateway ", version (:5435, :1322).
  OT-Direct sends that line to the port 25238 bridge only (Core reads a banner through processOT() as PIC detection).

These branches depend on much gateway state, so this is a source check on them, not a run; the bad-header SE (B3b) is
run through the real entry in tests/test_otd_bare_commands.py.
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_pr_gw_pic.py
"""
import os
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")


def branch(start, stop="\n  else if (cmd0 =="):
    i = SRC.index(start)
    return SRC[i:SRC.index(stop, i + len(start))]


class PrintSetting(unittest.TestCase):
    pr = branch("else if (cmd0 == 'P' && cmd1 == 'R') {")

    def test_exactly_one_letter_else_se(self):
        self.assertIn("if (strlen(value) != 1) reg = 0;", self.pr)
        self.assertIn('if (reg == 0) otDirectBridgeProcessStatus("SE");', self.pr)

    def test_unknown_letter_is_bv(self):
        default = self.pr[self.pr.index("      default:"):]
        self.assertIn('otDirectBridgeProcessStatus("BV")', default[:300])

    def test_e_is_the_pic_dash_without_external_sensor(self):
        self.assertIn('otDirectBridgeProcessPRResponse("PR: E=-")', self.pr)

    def test_f_has_no_case_so_it_is_bv(self):
        self.assertNotIn("case 'F':", self.pr)


class ResetBanner(unittest.TestCase):
    def test_gw_r_greets_on_the_bridge_only(self):
        gw = branch("else if (cmd0 == 'G' && cmd1 == 'W') {")
        r = gw[gw.index("} else if (value[0] == 'R') {"):]
        r = r[:r.index("} else {")]
        self.assertIn('PSTR("OpenTherm Gateway " OTD_PIC_COMPAT_VERSION " OTGW32")', r)
        self.assertIn("otDirectBridgeWriteLine(banner, strlen(banner))", r)
        self.assertNotIn("processOT(banner", r, "a banner through processOT() is the PIC detection in Core")


if __name__ == "__main__":
    unittest.main(verbosity=2)
