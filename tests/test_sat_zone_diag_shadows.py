#!/usr/bin/env python3
"""TASK-1465: SAT zone diagnostics publish on change through ADR-111 shadows.

sat/zone/<n>/{active,output,error} went out with a retained sendMQTTData() on every pass of the
control loop, bypassing the publish-if-changed shadows every other sat/* topic uses (ADR-111).
Now each zone has its own SATShadowB/SATShadowF per topic and the publishIfChanged* helpers decide.

Old-vs-fix: OTGW_SAT_INO points at another copy of SATcontrol.ino.

Run: python tests/test_sat_zone_diag_shadows.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))


class ZoneDiagShadows(unittest.TestCase):
    def setUp(self):
        src = SAT_INO.read_text(encoding="utf-8")
        m = re.search(r"^static void satPublishZoneDiagnostics\(\)\n\{.*?^\}\n", src, re.S | re.M)
        self.assertIsNotNone(m, "satPublishZoneDiagnostics() not found")
        self.fn = m.group(0)

    def test_no_unconditional_publish(self):
        self.assertNotIn("sendMQTTData(", self.fn)

    def test_every_zone_topic_goes_through_a_shadow(self):
        self.assertRegex(self.fn, r"static SATShadowB s_zoneActive\[SAT_MAX_ZONES\]")
        self.assertRegex(self.fn, r"publishIfChangedB\(topicBuf, active, s_zoneActive\[i\], true\)")
        self.assertRegex(self.fn, r"publishIfChangedF\(topicBuf, z\.fPidOutput, s_zoneOutput\[i\], SAT_EPS_PID_OUTPUT, 1, true\)")
        self.assertRegex(self.fn, r"publishIfChangedF\(topicBuf, z\.fSetpoint - z\.fRoomTemp, s_zoneError\[i\], SAT_EPS_ERROR, 2, true\)")

    def test_topics_unchanged(self):
        for t in ("sat/zone/%u/active", "sat/zone/%u/output", "sat/zone/%u/error"):
            self.assertIn(t, self.fn)


if __name__ == "__main__":
    unittest.main(verbosity=2)
