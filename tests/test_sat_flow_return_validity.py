#!/usr/bin/env python3
"""TASK-1412.5: validity and age of the boiler flow (MsgID 25) and return (MsgID 28) temperature in SAT.

Sergeant D entry 217 (QD-5 A): a return temperature invalid or absent for 5 minutes counts as no return temperature.
Entry 218 (QD-6 A): a flow temperature invalid or stale for N counts as missing (PWM idle, continuous setpoint); entry 241 (QD-6b A): N = 60 s.

Nothing is re-implemented. The host program is cut out of the production sources:
  - SATboilerFeed.h as it is (the feed classes),
  - the frame hook of processOT() in OTGW-Core.ino (the statement that stamps the feeds; the harness only models print_f88() storing
    the value, as the real code does for every boiler Read-Ack and answer frame),
  - the SATcontrol.ino block from "Flow (MsgID 25) and return (MsgID 28) feed validity" through satNoReturnTemp():
    satFlowFeedClass, satReturnFeedClass, satReturnFresh, satReturnMissingLong, satGetFlowTemp, satGetReturnTemp, satNoReturnTemp.
The device is simulated by stubs: the clock, the Unknown-DataID bitmap, the board return sensor and the stored Tboiler / Tret.

OTGW_SAT_INO and OTGW_CORE_INO point at other copies (e.g. `git show HEAD:...`) for the old-vs-fix run: the old code has no feed
block, so the harness cuts the old satGetFlowTemp / satGetReturnTemp / satNoReturnTemp and the frame hook does not exist.
Not reached by this host test: processOT() itself (decoding, the one-frame delay, bGatewaySubstituted pairing) and the PIC / OT-Direct
transports; that is for the bench (TASK-1412.5 AC3).

Run: python tests/test_sat_flow_return_validity.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
CORE_INO = Path(os.environ.get("OTGW_CORE_INO", FW / "OTGW-Core.ino"))
FEED_H = Path(os.environ.get("OTGW_FEED_H", FW / "SATboilerFeed.h"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
struct SatSettings { bool bSimulation = false; };
struct Settings { SatSettings sat; };
struct SatState { float fSimFlowTemp = 41.0f, fSimReturnTemp = 33.0f; };
struct OTBusStub { SatBoilerFeed feedTboiler; SatBoilerFeed feedTret; };
struct State { SatState sat; OTBusStub otBus; };
struct OTSys { float Tboiler = 0.0f, Tret = 0.0f; };
static Settings settings; static State state; static OTSys OTcurrentSystemState;
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static float satSimNoiseOffset() { return 0.0f; }
static bool g_unsup28 = false, g_unsup25 = false, g_confirmed = false, g_board = false;
static bool isBoilerMsgIdUnsupportedRead(uint8_t id) { return id == 28 ? g_unsup28 : (id == 25 ? g_unsup25 : false); }
static bool boilerMsgId28UnknownConfirmed() { return g_confirmed; }
static bool picBoardTretValid() { return g_board; }
static bool picBoardTretAbsent() { return !g_board; }
enum { OT_READ_DATA = 0, OT_READ_ACK = 4, OT_DATA_INVALID = 6, OT_UNKNOWN_DATA_ID = 7 };
enum { OT_Tboiler = 25, OT_Tret = 28 };
enum { OTGW_BOILER = 1, OTGW_THERMOSTAT = 2, OTGW_REQUEST_BOILER = 3, OTGW_ANSWER_THERMOSTAT = 4, OTGW_PARITY_ERROR = 5 };
struct OTd { int id = 0, type = 0, masterslave = 0, rsptype = 0; bool skipthis = false, bLocalAnswer = false, bReplayed = false, bGatewaySubstituted = false; };
static OTd OTdata;
"""

MAIN = r"""
// One frame: kind = ack | inv | unk | req | local | replay | cache | parity ; value only for ack-like kinds
static void frame(int id, const std::string& kind, float value) {
  OTdata = OTd();
  OTdata.id = id;
  if (kind == "ack")         { OTdata.type = OT_READ_ACK;         OTdata.masterslave = 1; OTdata.rsptype = OTGW_BOILER; }
  else if (kind == "inv")    { OTdata.type = OT_DATA_INVALID;     OTdata.masterslave = 1; OTdata.rsptype = OTGW_BOILER; }
  else if (kind == "unk")    { OTdata.type = OT_UNKNOWN_DATA_ID;  OTdata.masterslave = 1; OTdata.rsptype = OTGW_BOILER; }
  else if (kind == "req")    { OTdata.type = OT_READ_DATA;        OTdata.masterslave = 0; OTdata.rsptype = OTGW_THERMOSTAT; }
  else if (kind == "local")  { OTdata.type = OT_READ_ACK;         OTdata.masterslave = 1; OTdata.rsptype = OTGW_BOILER; OTdata.bLocalAnswer = true; }
  else if (kind == "replay") { OTdata.type = OT_READ_ACK;         OTdata.masterslave = 1; OTdata.rsptype = OTGW_BOILER; OTdata.bReplayed = true; }
  else if (kind == "cache")  { OTdata.type = OT_READ_ACK;         OTdata.masterslave = 1; OTdata.rsptype = OTGW_ANSWER_THERMOSTAT; OTdata.bLocalAnswer = true; }
  else if (kind == "parity") { OTdata.type = OT_READ_ACK;         OTdata.masterslave = 1; OTdata.rsptype = OTGW_PARITY_ERROR; OTdata.skipthis = true; }
  // print_f88(): a boiler Read-Ack or an answer frame writes the value (validForMaster); not a Data-Invalid, not a request, not a parity error
  if (OTdata.type == OT_READ_ACK && !OTdata.skipthis && OTdata.masterslave == 1) {
    if (id == 25) OTcurrentSystemState.Tboiler = value;
    if (id == 28) OTcurrentSystemState.Tret = value;
  }
#ifdef HAVE_FRAME_HOOK
  FRAME_HOOK
#endif
}
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream is(line);
    std::string cmd; is >> cmd;
    if (cmd == "t") { unsigned long v; is >> v; g_ms = (uint32_t)v; }
    else if (cmd == "f") { int id; std::string k; float v = 0; is >> id >> k >> v; frame(id, k, v); }
    else if (cmd == "unsup28") { int v; is >> v; g_unsup28 = v != 0; }
    else if (cmd == "unsup25") { int v; is >> v; g_unsup25 = v != 0; }
    else if (cmd == "confirmed") { int v; is >> v; g_confirmed = v != 0; }
    else if (cmd == "board") { int v; is >> v; g_board = v != 0; OTcurrentSystemState.Tret = v ? 31.0f : OTcurrentSystemState.Tret; }
    else if (cmd == "sim") { int v; is >> v; settings.sat.bSimulation = v != 0; }
    else if (cmd == "q") {
      char fc = '?', rc = '?'; unsigned long fage = 0, rage = 0;
#ifdef HAVE_FEED
      fc = satFeedClassLetter(satFlowFeedClass()); rc = satFeedClassLetter(satReturnFeedClass());
      fage = (unsigned long)satFlowSampleAgeMs(); rage = (unsigned long)satReturnSampleAgeMs();
#endif
      const char* rsrc = "?";
#ifdef HAVE_RSRC
      rsrc = satReturnSourceText();
#endif
      printf("flow=%.2f fc=%c fage=%lu ret=%.2f rc=%c rage=%lu rsrc=%s nr=%d\n", satGetFlowTemp(), fc, fage, satGetReturnTemp(), rc, rage, rsrc, (int)satNoReturnTemp());
    }
  }
  return 0;
}
"""


def build_source():
    sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    core = CORE_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    feed = FEED_H.read_text(encoding="utf-8").replace("\r\n", "\n").replace("#pragma once", "")
    marker = "// Flow (MsgID 25) and return (MsgID 28) feed validity"
    if marker in sat:
        a = sat.index(marker)
        defs = "#define HAVE_FEED 1\n"
        if "satReturnSourceText" in sat:
            defs += "#define HAVE_RSRC 1\n"
    else:   # old code: the wrappers read the stored value
        a = sat.index("static float satGetFlowTemp()")
        defs = ""
    b = sat.index("static bool satIsFlameOn()", a)
    block = sat[a:b]
    hook_start = core.find("if ((OTdata.id == OT_Tboiler || OTdata.id == OT_Tret)")
    if hook_start >= 0:
        hook_end = core.index("\n      }\n", hook_start) + len("\n      }\n")
        hook = core[hook_start:hook_end]
        defs += "#define HAVE_FRAME_HOOK 1\n#define FRAME_HOOK " + hook.replace("\n", " \\\n") + "\n"
    return feed + "\n" + STUBS + defs + "\n" + block + "\n" + MAIN


class FlowReturnValidity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_source(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def sim(self, script):
        """Run the script (one command per line); return one dict per `q` line."""
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe)], input="\n".join(script) + "\n", capture_output=True, text=True, check=True).stdout.splitlines()
        rows = []
        for l in out:
            r = dict(p.split("=", 1) for p in l.split())
            rows.append(r)
        return rows

    # ---- flow, entry 218 ---------------------------------------------------------------------------------------------
    def test_flow_never_received_is_missing(self):
        r, = self.sim(["t 5000", "q"])
        self.assertEqual((r["flow"], r["fc"]), ("0.00", "N"))

    def test_flow_valid_inside_60_s_stale_from_60_s_recovers_on_the_next_sample(self):
        r = self.sim(["t 1000", "f 25 ack 55.5", "q",
                      "t 60999", "q",                       # 59.999 s old
                      "t 61000", "q",                       # 60.000 s old: stale
                      "t 90000", "f 17 ack 40", "q",        # the link is live (another MsgID answers) but the flow is not
                      "t 91000", "f 25 ack 57.0", "q"])     # recovery
        self.assertEqual([(x["flow"], x["fc"]) for x in r],
                         [("55.50", "V"), ("55.50", "V"), ("0.00", "S"), ("0.00", "S"), ("57.00", "V")])
        self.assertEqual(r[0]["fage"], "0")
        self.assertEqual(r[1]["fage"], "59999")
        self.assertEqual(r[3]["fage"], "89000")

    def test_flow_59_s_old_is_valid_and_61_s_old_is_missing(self):   # entry 241: N = 60 s
        r = self.sim(["t 1000", "f 25 ack 55.5", "t 60000", "q", "t 62000", "q"])
        self.assertEqual([(x["flow"], x["fc"], x["fage"]) for x in r], [("55.50", "V", "59000"), ("0.00", "S", "61000")])

    def test_flow_data_invalid_is_missing_once_the_last_valid_sample_is_60_s_old_and_recovers(self):
        r = self.sim(["t 1000", "f 25 ack 55.5", "t 20000", "f 25 inv 0", "q",     # the old sample is 19 s old: still the last good value
                      "t 61000", "q",                                               # 60 s without a valid sample, the last reply was Data-Invalid
                      "t 62000", "f 25 ack 56.0", "q"])
        self.assertEqual([(x["flow"], x["fc"]) for x in r], [("55.50", "V"), ("0.00", "I"), ("56.00", "V")])

    def test_flow_data_invalid_without_any_sample_is_missing(self):
        r, = self.sim(["t 10000", "f 25 inv 0", "q"])
        self.assertEqual((r["flow"], r["fc"], r["fage"]), ("0.00", "I", "4294967295"))

    def test_flow_unknown_dataid_is_unsupported_and_a_later_ack_retracts_it(self):
        r = self.sim(["t 1000", "f 25 ack 50", "t 2000", "f 25 unk 0", "q", "t 3000", "f 25 ack 51", "q"])
        self.assertEqual([(x["flow"], x["fc"]) for x in r], [("0.00", "U"), ("51.00", "V")])

    def test_flow_unsupported_bitmap_from_a_previous_boot_counts_before_any_frame(self):
        r, = self.sim(["unsup25 1", "t 4000", "q"])
        self.assertEqual((r["flow"], r["fc"]), ("0.00", "U"))

    def test_flow_fresh_zero_is_its_own_class_and_reads_as_missing(self):
        r, = self.sim(["t 1000", "f 25 ack 0", "q"])
        self.assertEqual((r["flow"], r["fc"], r["fage"]), ("0.00", "Z", "0"))

    def test_only_a_boiler_reply_refreshes_the_flow(self):
        r = self.sim(["t 1000", "f 25 ack 55.5",
                      "t 70000", "f 25 req 0", "f 25 local 56", "f 25 replay 56", "f 25 cache 56", "f 25 parity 56", "q",
                      "t 71000", "f 25 ack 56", "q"])
        self.assertEqual([(x["flow"], x["fc"]) for x in r], [("0.00", "S"), ("56.00", "V")])

    def test_flow_age_survives_the_millis_wrap_and_a_stamp_at_millis_zero(self):
        r = self.sim(["t 4294967200", "f 25 ack 50", "t 200", "q", "t 60000", "q", "t 0", "f 25 ack 52", "t 5", "q"])
        self.assertEqual([(x["flow"], x["fc"]) for x in r], [("50.00", "V"), ("0.00", "S"), ("52.00", "V")])

    def test_flow_and_return_feeds_are_independent(self):
        r = self.sim(["t 1000", "f 28 ack 38", "t 50000", "q"])
        self.assertEqual((r[0]["fc"], r[0]["rc"]), ("N", "V"))

    # ---- return, entry 217 -------------------------------------------------------------------------------------------
    def test_return_absent_for_5_minutes_counts_as_no_return_temperature(self):
        r = self.sim(["t 299999", "q", "t 300000", "q"])
        self.assertEqual([(x["rc"], x["nr"]) for x in r], [("N", "0"), ("N", "1")])

    def test_return_valid_sample_counts_for_5_minutes_then_no_return_and_recovers(self):
        r = self.sim(["t 1000", "f 28 ack 38.5", "q",
                      "t 300999", "q",                      # 299.999 s old
                      "t 301000", "q",                      # 300 s old
                      "t 400000", "f 28 ack 39.0", "q"])
        self.assertEqual([(x["ret"], x["rc"], x["nr"]) for x in r],
                         [("38.50", "V", "0"), ("38.50", "V", "0"), ("0.00", "S", "1"), ("39.00", "V", "0")])

    def test_return_data_invalid_for_5_minutes_counts_as_no_return_temperature(self):
        r = self.sim(["t 1000", "f 28 ack 38.5", "t 100000", "f 28 inv 0", "q",    # one Data-Invalid does not end a good sample
                      "t 301000", "q"])
        self.assertEqual([(x["ret"], x["rc"], x["nr"]) for x in r], [("38.50", "V", "0"), ("0.00", "I", "1")])

    def test_return_data_invalid_without_a_sample_waits_for_the_5_minutes_since_boot(self):
        r = self.sim(["t 20000", "f 28 inv 0", "q", "t 300000", "q"])
        self.assertEqual([(x["rc"], x["nr"]) for x in r], [("I", "0"), ("I", "1")])

    def test_board_sensor_reading_keeps_the_return_and_its_loss_ends_it(self):
        r = self.sim(["t 1000", "f 28 ack 38.5", "t 400000", "board 1", "q", "board 0", "q"])
        self.assertEqual([(x["ret"], x["nr"]) for x in r], [("31.00", "0"), ("0.00", "1")])

    def test_unknown_dataid_with_no_board_sensor_is_no_return_at_once_as_before(self):
        r = self.sim(["unsup28 1", "confirmed 1", "t 2000", "q",
                      "confirmed 0", "q",                    # a restored verdict without evidence of this boot waits for the 5 minutes
                      "t 300000", "q"])
        self.assertEqual([x["nr"] for x in r], ["1", "0", "1"])
        self.assertEqual(r[0]["rc"], "U")

    def test_return_source_ot_board_only_and_none(self):   # TASK-1590: SATTICK rsrc
        r = self.sim(["t 1000", "f 28 ack 38.5", "q",                       # fresh MsgID 28 feed
                      "board 1", "q",                                       # both valid: OT wins
                      "t 400000", "q",                                      # OT stale, board still counted
                      "board 0", "q"])                                      # neither
        self.assertEqual([x["rsrc"] for x in r], ["OT", "OT", "BRD", "-"])
        self.assertEqual([x["ret"] for x in r], ["38.50", "31.00", "31.00", "0.00"])

    def test_return_source_board_when_the_boiler_answers_unknown_dataid(self):   # gruijter beta.49: rc=U rage=4294967295, ret from PR=E
        r, = self.sim(["unsup28 1", "confirmed 1", "board 1", "t 2000", "f 28 unk 0", "q"])
        self.assertEqual((r["rc"], r["rage"], r["rsrc"], r["ret"]), ("U", "4294967295", "BRD", "31.00"))

    def test_return_source_in_the_simulation_is_the_modelled_feed(self):
        r, = self.sim(["sim 1", "t 1000", "q"])
        self.assertEqual(r["rsrc"], "OT")

    def test_unknown_dataid_with_a_board_sensor_reading_is_not_no_return(self):
        r, = self.sim(["unsup28 1", "confirmed 1", "board 1", "t 2000", "q"])
        self.assertEqual(r["nr"], "0")

    def test_the_simulation_never_counts_as_no_return_and_reads_its_own_values(self):
        r, = self.sim(["sim 1", "t 900000", "q"])
        self.assertEqual((r["flow"], r["ret"], r["nr"], r["fc"], r["rc"]), ("41.00", "33.00", "0", "V", "V"))


if __name__ == "__main__":
    unittest.main()
