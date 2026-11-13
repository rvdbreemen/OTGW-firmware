"""TASK-1871: GET /api/v2/otgw/messages/<id> tells never seen from acked, unknown or invalid.

Before this task the endpoint returned {"label","value","unit"} only, so an ID the boiler was
never asked for and an ID the boiler rejected with Unknown-DataID looked the same (reporter
thaghostnl, #alpha-testing 2026-11-12). The fix records the last boiler answer per ID this boot
(recordBoilerAnswer) and reports status and last_seen_age_s.

The functional part cuts the REAL block between the 'begin/end boiler-answer record' markers out
of OTGW-Core.ino and compiles it on the host. The wiring is checked on the source text: the call
sits in processOT's B-frame branch under boilerEvidence (a gateway-made or replayed answer does
not count), and both REST senders emit the two fields. The live path is checked on the bench.
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

SRC = HERE.parent / "src" / "OTGW-firmware"
CORE = SRC / "OTGW-Core.ino"
REST = SRC / "restAPI.ino"
HDR = SRC / "OTGW-Core.h"

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#define PSTR(s) s
"""

MAIN = r"""
static void show(const char* tag, uint8_t id, uint32_t now) {
  printf("%s id=%u status=%s age=%d\n", tag, (unsigned)id, boilerAnswerStatus(id), (int)boilerAnswerAgeS(id, now));
}
int main() {
  show("start", 10, 5000);                                  // never asked
  recordBoilerAnswer(10, OT_UNKNOWN_DATA_ID, 6000);         // BF00A0000 Unknown-DataID after PM=10
  show("unknown", 10, 9000);
  recordBoilerAnswer(25, OT_READ_ACK, 7000);                // Tboiler answered
  show("ack", 25, 17500);
  recordBoilerAnswer(56, OT_WRITE_ACK, 0);                  // millis 0 must not read as never
  show("ack0", 56, 0);
  recordBoilerAnswer(27, OT_DATA_INVALID, 8000);
  show("invalid", 27, 8000);
  recordBoilerAnswer(10, OT_READ_ACK, 20000);               // a later ack replaces the unknown verdict
  show("later", 10, 21000);
  recordBoilerAnswer(14, 0 /* READ_DATA, a master frame */, 9000);
  show("master", 14, 10000);
  show("range", 200, 10000);
  recordBoilerAnswer(77, OT_READ_ACK, 0xFFFFF000u);         // millis wrap
  show("wrap", 77, 0x00001000u);
  return 0;
}
"""


def between(path: Path, begin: str, end: str) -> str:
    src = path.read_text(encoding="utf-8")
    a = src.find(begin)
    b = src.find(end)
    assert a >= 0 and b > a, f"markers not found in {path.name}"
    return src[a:b]


def func_body(src: str, signature: str) -> str:
    # The definition, not the prototype; the body ends at the first closing brace in column 0
    # (brace counting trips over comments such as "// root {").
    m = re.search(re.escape(signature) + r"\s*\{", src)
    assert m, f"{signature} definition not found"
    end = src.find("\n}", m.end())
    assert end > 0, f"end of {signature} not found"
    return src[m.end():end]


class BoilerAnswerRecord(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        block = between(CORE, "// begin boiler-answer record", "// end boiler-answer record")
        h = HDR.read_text(encoding="utf-8")
        consts = "".join(f"#define {n} {v}\n" for n, v in
                         re.findall(r"\b(OT_(?:READ_ACK|WRITE_ACK|DATA_INVALID|UNKNOWN_DATA_ID))\s*=\s*(0b[01]+)", h))
        m = re.search(r"#define OT_MSGID_MAX (\d+)", h)
        assert m and consts.count("#define") == 4, "OT message types / OT_MSGID_MAX not found in OTGW-Core.h"
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "rec.cpp"
        cpp.write_text(STUBS + consts + f"#define OT_MSGID_MAX {m.group(1)}\n" + block + MAIN, encoding="utf-8")
        cls.exe = base.compile_cpp(cpp, Path(cls.tmp.name) / ("rec" + (".exe" if os.name == "nt" else "")))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler on this host")
        out = subprocess.run([str(cls.exe)], capture_output=True, text=True, check=True).stdout
        cls.lines = {l.split()[0]: l for l in out.strip().splitlines()}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_never_asked_is_never_seen_with_age_minus_one(self):
        self.assertEqual(self.lines["start"], "start id=10 status=never_seen age=-1")

    def test_unknown_dataid(self):
        self.assertEqual(self.lines["unknown"], "unknown id=10 status=unknown_dataid age=3")

    def test_read_ack_and_age(self):
        self.assertEqual(self.lines["ack"], "ack id=25 status=ack age=10")

    def test_write_ack_at_millis_zero_is_seen(self):
        self.assertEqual(self.lines["ack0"], "ack0 id=56 status=ack age=0")

    def test_data_invalid(self):
        self.assertEqual(self.lines["invalid"], "invalid id=27 status=data_invalid age=0")

    def test_latest_answer_wins(self):
        self.assertEqual(self.lines["later"], "later id=10 status=ack age=1")

    def test_master_frame_type_is_not_an_answer(self):
        self.assertEqual(self.lines["master"], "master id=14 status=never_seen age=-1")

    def test_out_of_range_id(self):
        self.assertEqual(self.lines["range"], "range id=200 status=never_seen age=-1")

    def test_millis_wrap(self):
        self.assertEqual(self.lines["wrap"], "wrap id=77 status=ack age=8")


class Wiring(unittest.TestCase):
    def test_recorded_in_the_boiler_branch_under_boiler_evidence(self):
        src = CORE.read_text(encoding="utf-8")
        i = src.find("const bool boilerEvidence = !OTdata.bLocalAnswer && !OTdata.bReplayed;")
        self.assertGreaterEqual(i, 0)
        window = src[i:i + 600]
        self.assertRegex(window, r"if \(boilerEvidence\) recordBoilerAnswer\(OTdata\.id, OTdata\.type, millis\(\)\);")

    def test_both_rest_senders_emit_status_and_age(self):
        src = REST.read_text(encoding="utf-8")
        for sig in ("void sendOTValue(int msgid)", "void sendOTLabel(const char *msglabel)"):
            body = func_body(src, sig)
            self.assertIn('je.field(F("status"), FPSTR(boilerAnswerStatus(', body, sig)
            self.assertIn('je.field(F("last_seen_age_s"), boilerAnswerAgeS(', body, sig)
            self.assertIn('je.field(F("unit")', body, sig)   # existing fields stay


if __name__ == "__main__":
    unittest.main()
