#!/usr/bin/env python3
"""TASK-1412.4 AC3: host traces prove the REQUESTED master bits and the TRANSMITTED master bits separately.

Requested = what SAT asks for each control tick, from the REAL satApplyPWM / satChEnableDecide (SATcontrol.ino, SATchEnable.h):
  CH enable (MsgID 0 master bit 0) and CS. The only master bit SAT drives is bit 0; the low-load work itself (PWM ON/OFF, the
  slave's permission) never touches another master bit. MM travels in MsgID 14 and is listed at the end.
Transmitted on OT-Direct = what the REAL otdEffectiveMasterFlags / otdRelayedStatusFrame / applyOverrides put into the MsgID 0 frame
  that reaches the boiler (OTDirect.ino, ADR-208), and what the thermostat is told back (high byte stays its own).
Transmitted on a PIC board = produced by the PIC firmware (gateway.asm), which is not part of this repository's code and is not run
  here. What the ESP side can show of it is the R frame; that routing is checked as a source pattern only. The PIC wire bits stay
  a PH-PIC bench check (docs/sat/sat-low-load-verification-plan-2026-10-28.md).

No firmware trace (state/log/REST of requested-versus-transmitted) is added: the AC asks for host traces, the OT-Direct frame is
reachable on the host through the real functions, and a new published field would need its own decision on names and units.

Safe fallback and re-entry, where the code and the recorded entries already define them (all asserted below as source patterns):
  * PWM OFF phase and cold cutoff: CS=10 together with CH=0 (entries 5, 9, 17; SATchEnable.h satChEnableDecide).
  * Slave withdraws low-load permission (MsgID 3 HB bit 4 = 1): PWM -> continuous in that tick, CH=1 (entry 214; satCycleCheckLowLoadVeto,
    SATcycles.ino); re-entry only on a valid reply with bit 4 = 0 through the normal entry rules (entries 213, 215; satPwmEntryAllowed).
  * No boiler reply for 30 s: hand-back CS=0, CH=1, MM=T once, then nothing until a reply returns (entry 47; satOtLinkStep, SATcontrol.ino).
  * SAT switched off or tripped: the same hand-back (satDisable / satSendRelease, TASK-1346, 1344, 1574).
  * PIC restart: the MM= and CH= it forgot are sent again (satNotifyPicRestart, TASK-1570), and an unchanged value is repaired after 60 s
    (entry 239).
  * OT-Direct: CH= acts only while a CS override is active (PIC CHModeOff, gateway.asm:53), so a hand-back with CS=0 returns bit 0 to the
    thermostat's own value whatever CH= was left at.

Run: python tests/test_sat_requested_vs_transmitted.py
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent
FW = REPO / "src" / "OTGW-firmware"
sys.path.insert(0, str(TESTS))
import test_otd_status_relay as otr  # noqa: E402
import test_sat_hp_pwm as hp  # noqa: E402
import test_sat_telemetry_wire as tw  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

SAT = (FW / "SATcontrol.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
CYC = (FW / "SATcycles.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
CHH = FW / "SATchEnable.h"

TRUTH_MAIN = r'''
#include <cstdio>
#include "@CHH@"
int main() {
  const float cold = 28.2f;
  const float reqs[] = {20.0f, 28.2f, 40.0f};
  for (float req : reqs)
    for (int drives = 0; drives < 2; drives++)
      for (int on = 0; on < 2; on++)
        printf("req=%.1f drives=%d on=%d ch=%u\n", req, drives, on, (unsigned)satChEnableDecide(req, cold, drives != 0, on != 0));
  return 0;
}
'''


class RequestedVsTransmitted(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        old = hp.MAIN
        hp.MAIN = tw.PWM_MAIN
        try:
            pwm = hp.build(d)
        finally:
            hp.MAIN = old
        cls.rows = None
        if pwm:
            out = subprocess.run([str(pwm)], capture_output=True, text=True, timeout=60).stdout
            cls.rows = [{k: float(v) for k, v in (p.split("=") for p in ln.split())} for ln in out.splitlines()]
        cpp = d / "truth.cpp"
        cpp.write_text(TRUTH_MAIN.replace("@CHH@", CHH.as_posix()), encoding="utf-8")
        t = compile_cpp(cpp, d / "truth.exe")
        cls.truth = None
        if t:
            out = subprocess.run([str(t)], capture_output=True, text=True, timeout=20).stdout
            cls.truth = [dict(p.split("=") for p in ln.split()) for ln in out.splitlines()]
        otr.Compiled.setUpClass()
        cls.relay = otr.Compiled('test_a_cs_held_ch0_clears_bit0_with_valid_parity') if otr.Compiled.exe else None

    @classmethod
    def tearDownClass(cls):
        otr.Compiled.tearDownClass()
        cls.dir.cleanup()

    def need(self):
        if self.rows is None or self.truth is None or self.relay is None:
            self.skipTest("no host C++ compiler")

    # ---- requested ----------------------------------------------------------------------------------------------------------
    def test_requested_ch_rule_on_the_real_header(self):
        self.need()
        want = {}
        for r in self.truth:
            req, drives, on = float(r["req"]), int(r["drives"]), int(r["on"])
            expect = 0 if req <= 28.2 else (0 if (drives and not on) else 1)
            self.assertEqual(int(r["ch"]), expect, r)
            want[(req, drives, on)] = int(r["ch"])
        self.assertEqual(want[(40.0, 1, 1)], 1, "PWM ON: CH enable 1")
        self.assertEqual(want[(40.0, 1, 0)], 0, "PWM OFF: CH enable 0 (with CS=10)")
        self.assertEqual(want[(40.0, 0, 0)], 1, "a held CS (DHW, no flow, overshoot protection off) keeps CH=1")
        self.assertEqual(want[(28.2, 0, 1)], 0, "cold cutoff: CH enable 0")

    def test_requested_trace_of_a_pwm_run_ch_follows_the_phase_and_cs_is_10_whenever_ch_is_0(self):
        self.need()
        phases = {(int(r["ch"]), r["cs"] == 10.0) for r in self.rows}
        self.assertIn((1, False), phases)
        self.assertIn((0, True), phases)
        self.assertNotIn((0, False), phases, "SAT never requests CH=0 with a CS above the minimum")
        self.assertNotIn((1, True), phases, "CS=10 only goes out in the OFF phase")
        self.assertGreaterEqual(sum(1 for a, b in zip(self.rows, self.rows[1:]) if a["ch"] == 1 and b["ch"] == 0), 3, "several ON->OFF edges")

    # ---- transmitted on OT-Direct, separate from requested --------------------------------------------------------------------
    def test_transmitted_bit0_equals_requested_ch_on_every_tick_of_the_run(self):
        self.need()
        table = []
        for r in self.rows:
            requested = int(r["ch"])
            cs_active = r["cs"] != 0
            for thermostat_bit0 in (0, 1):
                data = (0x02 << 8) | (thermostat_bit0 << 8)   # master HB: DHW enable + the thermostat's own CH bit
                sent, modified, parity, reply, rparity, _ = self.relay.run_case(cs_active, requested == 0, 1, 0, 0, data, 0x020A)
                transmitted = (sent >> 8) & 1
                table.append((requested, transmitted, thermostat_bit0, (reply >> 8) & 1))
                self.assertEqual(parity, 1, "parity of the frame sent to the boiler")
                self.assertEqual(rparity, 1)
                self.assertEqual(transmitted, requested, "tick t=%d: requested CH %d, transmitted bit 0 %d" % (r["t"], requested, transmitted))
                self.assertEqual((reply >> 8) & 1, thermostat_bit0, "the thermostat is told its own CH bit back, not the transmitted one")
        self.assertTrue(any(t[0] != t[2] for t in table), "the run has ticks where requested and the thermostat's own bit differ")

    def test_only_bit0_of_the_master_byte_is_ever_rewritten(self):
        self.need()
        for hb in range(256):
            for ch_off in (False, True):
                sent, _, parity, _, _, _ = self.relay.run_case(True, ch_off, 1, 0, 0, hb << 8)
                self.assertEqual(((sent >> 8) & 0xFF) & ~1, hb & ~1, "master HB %02X: bits 1..7 untouched" % hb)
                self.assertEqual(sent & 0xFF, 0, "low byte of a READ_DATA stays as sent")
                self.assertEqual(parity, 1)

    def test_a_ch0_request_without_a_cs_override_is_not_transmitted(self):
        """Requested and transmitted differ here, and the code says why: CH= only counts while CS is overridden (gateway.asm:53). SAT
        sends CS before CH in every tick (SATcontrol.ino satSendCS, satSendMM, satSendCH order) and the OFF phase CS is 10, never 0."""
        self.need()
        for thermostat_bit0 in (0, 1):
            sent, modified, _, _, _, _ = self.relay.run_case(False, True, 1, 0, 0, thermostat_bit0 << 8)
            self.assertEqual((sent >> 8) & 1, thermostat_bit0, "CH=0 stays unsent without a CS override")
            self.assertEqual(modified, 0)
        order = [SAT.index(x) for x in ("satSendCS(finalSetpoint);", "satSendMM(mmTarget);", "satSendCH(satChEnableDecide(")]
        self.assertEqual(order, sorted(order), "CS, MM, CH in this order inside one tick")

    def test_hand_back_returns_bit0_to_the_thermostat_whatever_ch_was_left_at(self):
        """satSendRelease: CS=0, CH=1, MM=T. After CS=0 the override is off, so a stale CH=0 has no effect either."""
        self.need()
        for left_ch_off in (False, True):
            for thermostat_bit0 in (0, 1):
                sent, modified, _, _, _, _ = self.relay.run_case(False, left_ch_off, 1, 0, 0, thermostat_bit0 << 8)
                self.assertEqual((sent >> 8) & 1, thermostat_bit0)
        rel = re.search(r"static bool satSendRelease\(bool restore\)\s*\{.*?^\}", SAT, re.S | re.M).group(0)
        for cmd in ('"CS=0"', '"CH=1"', '"MM=T"'):
            self.assertIn(cmd, rel)

    # ---- transmitted on a PIC board: what the ESP side can see ----------------------------------------------------------------
    def test_pic_board_the_esp_reads_the_transmitted_master_byte_from_the_r_frame(self):
        """The canonical status the REST/MQTT show comes from the frame that reached the boiler: a thermostat T frame the gateway
        replaced is flagged bGatewaySubstituted and kept off the canonical topics; the R frame carries the bit that went out. The PIC's
        own rewriting is not run here (PH-PIC bench check)."""
        core = (FW / "OTGW-Core.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertRegex(core, r"if \(OT\.rsptype == OTGW_THERMOSTAT && OT\.bGatewaySubstituted\) return false;")
        self.assertRegex(core, r"\(\(OTdata\.rsptype == OTGW_REQUEST_BOILER\) && \(delayedOTdata\.rsptype == OTGW_THERMOSTAT\)\)")
        self.assertRegex(core, r"bool isCentralHeatingEnabled\(\)\s*\{\s*return OTcurrentSystemState\.MasterStatus & 0x01;")

    # ---- safe fallback and re-entry: where they are defined ---------------------------------------------------------------------
    def test_veto_falls_back_to_continuous_in_the_same_tick_and_ch_is_1_there(self):
        body = re.search(r"bool satCycleCheckLowLoadVeto\(\)\s*\{.*?^\}", CYC, re.S | re.M).group(0)
        self.assertIn("satSetControlMode(SAT_MODE_CONTINUOUS)", body)
        self.assertIn("satPwmPermitted()", body)
        # continuous: pwmDrivesSetpoint false, so CH=1 unless the cold cutoff holds (truth table above)
        self.need()
        self.assertTrue(any(r["req"] == "40.0" and r["drives"] == "0" and r["ch"] == "1" for r in self.truth))
        self.assertIn("satPwmEntryAllowed(", CYC)   # re-entry only through the entry gate (entries 213, 215)

    def test_lost_link_stop_restart_and_repair_paths_exist(self):
        self.assertIn("SAT: no boiler reply for 30 s, boiler handed back (CS=0, CH=1, MM=T)", SAT)
        self.assertRegex(SAT, r"void satNotifyPicRestart\(\)")
        self.assertRegex(SAT, r"SAT_MMCH_REPAIR_MS")
        self.assertRegex(SAT, r"void satDisable\(bool persistPid\)")

    # ---- MM travels in MsgID 14 --------------------------------------------------------------------------------------------------
    def test_mm_unit_on_ot_direct_is_percent_as_f8_8_and_mm_t_clears_the_override(self):
        od = (FW / "OTDirect.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        seg = od[od.index("cmd0 == 'M' && cmd1 == 'M'"):][:700]
        self.assertIn("floatToF88(atof(value))", seg)
        self.assertIn("enqueueWriteCommand(14, f88", seg)
        self.assertIn("clearWriteOverride(14)", seg)


if __name__ == "__main__":
    unittest.main(verbosity=2)
