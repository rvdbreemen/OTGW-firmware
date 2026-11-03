#!/usr/bin/env python3
"""TASK-1412.7 AC3 / LL-012: SAT's master CH enable and the boiler's CH-mode bit are two different signals.

MsgID 0 carries both. The MASTER byte (Statusflags bit 8, 0x0100) is what SAT itself asks for: CH enable, sent by
satChEnableDecide() and cleared in every PWM OFF phase (entries 5, 9, 17; SATchEnable.h). The SLAVE byte bit 1 (0x0002) is what the
boiler reports: CH mode, which stays 1 through the pump overrun after SAT cleared the enable (Sergeant D, entry 219, QD-7 A;
Robert, entry 201). The low-load entry and exits (satCycleCheckOvershootSwitch / satCycleCheckPwmExits) and the CH-enable decision
read neither bit; they are not allowed to start doing so without this test and LL-012 being revisited.

Tests (real code, no re-implementation):
  * the CH-enable rule: tests/test_sat_ch_enable.cpp compiled and run against the real SATchEnable.h, plus the PWM OFF cell below;
  * an inventory of every reader of the two bits in SAT*.ino|h, mapped to the enclosing function, so a new consumer fails here until
    it is classified (master / slave) and its test is named;
  * the sustained switches and the PWM suppression read no CH bit at all.

The behavioural tests of the two signals live in the suites LL-012 delegates to:
  tests/test_sat_cycle_classifier.py::test_R2_master_ch_enable_decides_the_kind   (slave CH mode alone is not CH, master bit decides)
  tests/test_sat_pressure_health.py::test_pwm_off_phase_does_not_suspend         (master off, slave on: the window is NOT reset)
  tests/test_sat_pressure_health.py (suspend on a slave-bit change)

Run: python tests/test_sat_ch_signals.py
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "src" / "OTGW-firmware"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

MASTER = re.compile(r"0x0100\b")
SLAVE = re.compile(r"Statusflags\s*&\s*0x0*2\b|SlaveStatus\s*&\s*0x0*2\b|isCentralHeatingActive\s*\(")
FUNC = re.compile(r"^(?:static\s+|inline\s+)*[A-Za-z_][\w:<>\*& ]*?\b(\w+)\s*\([^;{]*\)\s*(?:const)?\s*\{?\s*$")

# Where the boiler's CH-mode bit (slave) is read, and what each reader means:
#   _cycleTailSample / satCycleSample is the only MASTER reader: the cycle kind (tests/test_sat_cycle_classifier.py R2).
#   satCycleOnFlameChange: off-with-demand duration, telemetry only (docs/sat/firmware-sat-spec.md, `central_heating` row).
#   satUpdatePressure: pressure-window reset (tests/test_sat_pressure_health.py, entry 201).
#   satPublishMQTT: power / sync diagnostics (sat/ch_sync), telemetry only.


def readers(path: Path, pattern):
    out = []
    lines = path.read_text(encoding="utf-8").replace("\r\n", "\n").split("\n")
    func = None
    for i, ln in enumerate(lines):
        if ln and not ln[0].isspace() and not ln.startswith(("//", "#", "}")):
            m = FUNC.match(ln.rstrip())
            if m and m.group(1) not in ("if", "for", "while", "switch"):
                func = m.group(1)
        code = ln.split("//")[0]
        if pattern.search(code):
            out.append((i + 1, func, ln.strip()))
    return out


class ChSignals(unittest.TestCase):
    def files(self):
        return sorted(p for p in FW.glob("SAT*.*") if p.suffix in (".ino", ".h") and p.name != "SATble.ino")

    def test_master_ch_enable_has_one_reader_the_cycle_sampler(self):
        found = [(p.name, f, n) for p in self.files() for n, f, _ in readers(p, MASTER)]
        self.assertEqual([(p, f) for p, f, _ in found], [("SATcycles.ino", "_cycleTailSample")], found)

    def test_slave_ch_mode_readers_are_the_three_classified_ones(self):
        found = {(p.name, f) for p in self.files() for _, f, _ in readers(p, SLAVE)}
        known = set(EXPECTED_SLAVE)
        self.assertEqual(found, known, "a new reader of the boiler's CH-mode bit (or a moved one): classify it as master or slave, "
                                       "name its test, then update EXPECTED_SLAVE in tests/test_sat_ch_signals.py: %s" % sorted(found ^ known))

    def test_low_load_entry_exits_and_ch_enable_read_no_ch_bit(self):
        cyc = (FW / "SATcycles.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        for head in ("bool satCycleCheckOvershootSwitch(", "bool satCycleCheckPwmExits("):
            a = cyc.index(head)
            b = cyc.index("\n}\n", a)
            body = cyc[a:b]
            self.assertFalse(MASTER.search(body) or SLAVE.search(body) or "Statusflags" in body or "MasterStatus" in body, head)
        ch = (FW / "SATchEnable.h").read_text(encoding="utf-8")
        fn = ch[ch.index("static inline uint8_t satChEnableDecide("):]
        fn = fn[:fn.index("\n}\n")]
        self.assertNotIn("Statusflags", fn)
        self.assertNotIn("SlaveStatus", fn)

    def test_ch_enable_rule_binary_is_green_and_pwm_off_clears_ch(self):
        d = tempfile.TemporaryDirectory()
        try:
            exe = compile_cpp(REPO / "tests" / "test_sat_ch_enable.cpp", Path(d.name) / "ch.exe")
            if not exe:
                self.skipTest("no host C++ compiler")
            r = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout[-800:])
            self.assertIn("PWM OFF driving, req 35", r.stdout)   # the OFF-phase cell exists: master CH enable 0 while the pump may still run
        finally:
            d.cleanup()


EXPECTED_SLAVE = {("SATcycles.ino", "satCycleOnFlameChange"), ("SATcontrol.ino", "satUpdatePressure"),
                  ("SATcontrol.ino", "satPublishMQTT")}

if __name__ == "__main__":
    unittest.main(verbosity=2)
