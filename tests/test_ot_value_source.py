"""TASK-1965: GET /api/v2/otgw/messages/<id> and /otgw/label/<label> say where the served value came from.

Reporter indigo_light (beta.250, Classic + PIC 6.8 with a DS18B20, Vitodens without MsgID 27): label
Toutside returned value 6.31 (the PIC sensor, which SAT uses) with status data_invalid. status is the
boiler's last answer (TASK-1871) while value is what the gateway serves, so a client read a valid value
as invalid. The fix keeps status as it is and adds source: boiler / gateway / thermostat / summary /
replay / none, recorded where the canonical value is written.

The functional part cuts the REAL block between the 'begin/end boiler-answer record' markers out of
OTGW-Core.ino (with the enums from OTGW-Core.h and OTGW-firmware.h) and runs indigo's case on the host:
boiler Data-Invalid for 27 plus the gateway's own Write-Data 27. The TASK-1871 results stay as they were.
The wiring is checked on the source text: the record sits after decodeAndPublishOTValue() under the same
store gate the print_* functions use, the PS=1 summary and OT-Direct's OT= record their source, and both
REST senders emit the field. The live path is checked on the bench / in the field.
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import test_sat_pwm_override as base  # noqa: E402  (compile_cpp: g++, clang++ or MSVC)

SRC = Path(os.environ.get("OTGW_SRC_DIR", HERE.parent / "src" / "OTGW-firmware"))
CORE = SRC / "OTGW-Core.ino"
REST = SRC / "restAPI.ino"
OTD = SRC / "OTDirect.ino"
HDR = SRC / "OTGW-Core.h"
FWH = SRC / "OTGW-firmware.h"

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#define PSTR(s) s
"""

MAIN = r"""
static void show(const char* tag, uint8_t id) {
  printf("%s id=%u status=%s source=%s\n", tag, (unsigned)id, boilerAnswerStatus(id), valueSourceName(id));
}
int main() {
  show("never", 27);
  // indigo_light: the thermostat reads 27, the PIC turns it into a Write-Data 27 with its DS18B20 value
  // (R frame, stored as Toutside), the Vitodens answers Data-Invalid (B frame, no value stored).
  recordValueSource(27, otValueSourceOfFrame(OTGW_REQUEST_BOILER, false, false));
  recordBoilerAnswer(27, OT_DATA_INVALID, 1000);
  show("indigo", 27);
  recordBoilerAnswer(25, OT_READ_ACK, 1000);
  recordValueSource(25, otValueSourceOfFrame(OTGW_BOILER, false, false));
  show("boiler", 25);
  recordValueSource(26, otValueSourceOfFrame(OTGW_BOILER, true, false));        // loopback / local B
  show("localB", 26);
  recordValueSource(28, otValueSourceOfFrame(OTGW_ANSWER_THERMOSTAT, false, false));
  show("answer", 28);
  recordValueSource(24, otValueSourceOfFrame(OTGW_THERMOSTAT, false, false));
  show("thermo", 24);
  recordValueSource(17, otValueSourceOfFrame(OTGW_BOILER, false, true));
  show("replay", 17);
  recordValueSource(18, OT_VALSRC_SUMMARY);
  show("summary", 18);
  recordValueSource(27, otValueSourceOfFrame(OTGW_BOILER, false, false));       // a later boiler value wins
  show("later", 27);
  recordValueSource(200, OT_VALSRC_BOILER);
  show("range", 200);
  return 0;
}
"""


def between(path: Path, begin: str, end: str) -> str:
    src = path.read_text(encoding="utf-8")
    a, b = src.find(begin), src.find(end)
    assert a >= 0 and b > a, f"markers not found in {path.name}"
    return src[a:b]


class ValueSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        block = between(CORE, "// begin boiler-answer record", "// end boiler-answer record")
        h = HDR.read_text(encoding="utf-8")
        consts = "".join(f"#define {n} {v}\n" for n, v in
                         re.findall(r"\b(OT_(?:READ_ACK|WRITE_ACK|DATA_INVALID|UNKNOWN_DATA_ID))\s*=\s*(0b[01]+)", h))
        m = re.search(r"#define OT_MSGID_MAX (\d+)", h)
        rsp = re.search(r"enum OTGW_response_type\s*\{[^}]*\};", h)
        assert m and consts.count("#define") == 4 and rsp, "OT types, OT_MSGID_MAX or OTGW_response_type not found"
        src_enum = re.search(r"enum : uint8_t \{ OT_VALSRC_NONE[^}]*\};", FWH.read_text(encoding="utf-8"))
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "src.cpp"
        cpp.write_text(STUBS + consts + f"#define OT_MSGID_MAX {m.group(1)}\n" + rsp.group(0) + "\n"
                       + (src_enum.group(0) + "\n" if src_enum else "") + block + MAIN, encoding="utf-8")
        cls.exe = base.compile_cpp(cpp, Path(cls.tmp.name) / ("src" + (".exe" if os.name == "nt" else "")))
        if cls.exe is None:   # a failed compile raises in compile_cpp (red); None means no compiler at all
            raise unittest.SkipTest("no C++ compiler on this host")
        out =subprocess.run([str(cls.exe)], capture_output=True, text=True, check=True).stdout
        cls.lines = {l.split()[0]: l for l in out.strip().splitlines()}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def line(self, tag):
        self.assertIsNotNone(self.lines, "the boiler-answer block did not compile with the value source record")
        return self.lines[tag]

    def test_indigo_gateway_value_with_boiler_data_invalid(self):
        self.assertEqual(self.line("indigo"), "indigo id=27 status=data_invalid source=gateway")

    def test_never_written_is_none(self):
        self.assertEqual(self.line("never"), "never id=27 status=never_seen source=none")

    def test_boiler_value(self):
        self.assertEqual(self.line("boiler"), "boiler id=25 status=ack source=boiler")

    def test_gateway_made_frames(self):
        self.assertEqual(self.line("localB"), "localB id=26 status=never_seen source=gateway")
        self.assertEqual(self.line("answer"), "answer id=28 status=never_seen source=gateway")

    def test_thermostat_replay_and_summary(self):
        self.assertEqual(self.line("thermo"), "thermo id=24 status=never_seen source=thermostat")
        self.assertEqual(self.line("replay"), "replay id=17 status=never_seen source=replay")
        self.assertEqual(self.line("summary"), "summary id=18 status=never_seen source=summary")

    def test_latest_writer_wins_and_range(self):
        self.assertEqual(self.line("later"), "later id=27 status=data_invalid source=boiler")
        self.assertEqual(self.line("range"), "range id=200 status=never_seen source=none")


class Wiring(unittest.TestCase):
    def test_recorded_after_decode_under_the_store_gate(self):
        src = CORE.read_text(encoding="utf-8")
        i = src.find("        decodeAndPublishOTValue();")
        self.assertGreater(i, 0)
        window = src[i:i + 600]
        self.assertRegex(window, r"if \(is_value_valid\(OTdata, OTlookupitem\) && is_value_valid_for_master_topic\(OTdata, OTlookupitem\)\)\s*"
                                 r"recordValueSource\(OTdata\.id, otValueSourceOfFrame\(OTdata\.rsptype, OTdata\.bLocalAnswer, OTdata\.bReplayed\)\);")

    def test_summary_and_ot_direct_ot_record_their_source(self):
        self.assertIn("if (validForMaster) recordValueSource(msgid, OT_VALSRC_SUMMARY);", CORE.read_text(encoding="utf-8"))
        otd = OTD.read_text(encoding="utf-8")
        i = otd.find("OTcurrentSystemState.Toutside = (int16_t)f88 / 256.0f;")
        self.assertGreater(i, 0)
        self.assertIn("recordValueSource(27, OT_VALSRC_GATEWAY);", otd[i:i + 200])

    def test_both_rest_senders_emit_source(self):
        src = REST.read_text(encoding="utf-8")
        self.assertEqual(src.count('je.field(F("source"), FPSTR(valueSourceName((uint8_t)msgid)));'), 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
