"""TASK-1896: which OpenTherm frames stamp the MsgID 27 outside temperature as fresh for SAT.

Robert decided (entry 330) that an outside temperature the gateway puts on the bus itself counts,
like the boiler's own Read-Ack. A PIC with a DS18B20 (TS=O) or an OT= value turns the thermostat's
Read-Data 27 into a Write-Data 27 towards the boiler (gateway.asm 6.8, MessageID27): the R frame
carries the value and is what writes the canonical Toutside (OT_RW, is_value_valid_for_master_topic).
The boiler then answers Write-Ack or Unknown-DataId, never Read-Ack, so the old stamp never fired.

The test cuts the real predicate otFrameIsFreshOutsideTemp() (the "// --- otOutsideStamp begin" ..
"end" block) out of OTGW-Core.ino and compiles it with a host C++ compiler; nothing is
re-implemented. OTGW_CORE_INO=<copy of an older OTGW-Core.ino> runs it against other code.
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

CORE_INO = Path(os.environ.get("OTGW_CORE_INO", REPO / "src" / "OTGW-firmware" / "OTGW-Core.ino"))

# Same names and values as OTGW-Core.h (OTlibMessageType :222-230, OTGW_response_type :796-803).
STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
typedef uint8_t byte;
enum { OT_READ_DATA = 0, OT_WRITE_DATA = 1, OT_INVALID_DATA = 2, OT_RESERVED = 3,
       OT_READ_ACK = 4, OT_WRITE_ACK = 5, OT_DATA_INVALID = 6, OT_UNKNOWN_DATA_ID = 7 };
enum OTGW_response_type { OTGW_BOILER, OTGW_THERMOSTAT, OTGW_ANSWER_THERMOSTAT, OTGW_REQUEST_BOILER,
                          OTGW_PARITY_ERROR, OTGW_UNDEF };
struct OpenthermData_t { byte type = 0, id = 0, rsptype = 0, skipthis = 0, bGatewaySubstituted = 0,
                         bAnswerOverride = 0, bReplayed = 0, bLocalAnswer = 0; };
"""

MAIN = r"""
int main(int argc, char **argv) {
  // argv: id rsptype type skipthis substituted answerOverride replayed localAnswer
  OpenthermData_t f;
  f.id = (byte)atoi(argv[1]); f.rsptype = (byte)atoi(argv[2]); f.type = (byte)atoi(argv[3]);
  f.skipthis = (byte)atoi(argv[4]); f.bGatewaySubstituted = (byte)atoi(argv[5]);
  f.bAnswerOverride = (byte)atoi(argv[6]); f.bReplayed = (byte)atoi(argv[7]);
  f.bLocalAnswer = (byte)atoi(argv[8]);
  printf("%d\n", otFrameIsFreshOutsideTemp(f) ? 1 : 0);
  return 0;
}
"""

B, T, A, R, P = 0, 1, 2, 3, 4
READ_DATA, WRITE_DATA, INVALID_DATA, READ_ACK, WRITE_ACK, DATA_INVALID, UNKNOWN = 0, 1, 2, 4, 5, 6, 7


def _block(src: str) -> str:
    m = re.search(r"// --- otOutsideStamp begin\n(.*?)// --- otOutsideStamp end", src, re.S)
    if not m:
        raise AssertionError("otOutsideStamp block not found in " + str(CORE_INO))
    return m.group(1)


class OutsideStampFrames(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = CORE_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls._dir = tempfile.TemporaryDirectory()
        cpp = Path(cls._dir.name) / "t.cpp"
        cpp.write_text(STUBS + _block(src) + MAIN, encoding="utf-8")
        cls._exe = compile_cpp(cpp, Path(cls._dir.name) / "t.exe")
        if cls._exe is None:
            raise unittest.SkipTest("no host C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls._dir.cleanup()

    def stamp(self, rsptype, mtype, msgid=27, skip=0, subst=0, override=0, replayed=0, local=0):
        out = subprocess.run([str(self._exe), str(msgid), str(rsptype), str(mtype), str(skip), str(subst),
                              str(override), str(replayed), str(local)], capture_output=True, text=True, check=True)
        return out.stdout.strip() == "1"

    # The boiler's own value (unchanged since TASK-1390 / TASK-1649).
    def test_boiler_read_ack_stamps(self):
        self.assertTrue(self.stamp(B, READ_ACK))

    def test_boiler_read_ack_replayed_parity_or_substituted_does_not_stamp(self):
        self.assertFalse(self.stamp(B, READ_ACK, replayed=1))
        self.assertFalse(self.stamp(B, READ_ACK, skip=1))
        self.assertFalse(self.stamp(B, READ_ACK, subst=1))

    # TASK-1896 follow-up (1903 item 2): a B Read-Ack 27 the gateway made itself (OT-Direct loopback, bLocalAnswer, OTGW-Core.ino processOT)
    # is neither the boiler's value nor a value the gateway measured (entries 57, 330): no stamp.
    def test_local_answer_boiler_read_ack_does_not_stamp(self):
        self.assertFalse(self.stamp(B, READ_ACK, local=1))

    # TASK-1896: the gateway's own value, the R frame Write-Data 27 (PIC DS18B20 or OT=).
    def test_gateway_write_data_to_boiler_stamps(self):
        self.assertTrue(self.stamp(R, WRITE_DATA))

    def test_gateway_write_data_replayed_or_parity_does_not_stamp(self):
        self.assertFalse(self.stamp(R, WRITE_DATA, replayed=1))
        self.assertFalse(self.stamp(R, WRITE_DATA, skip=1))

    def test_failed_sensor_invalid_data_does_not_stamp(self):
        # PIC SensorInvalid -> OutsideInvalid sends an Invalid-Data request (gateway.asm 6.8 MessageID27).
        self.assertFalse(self.stamp(R, INVALID_DATA))

    def test_frames_without_a_new_measurement_do_not_stamp(self):
        self.assertFalse(self.stamp(T, READ_DATA))              # the thermostat's question
        self.assertFalse(self.stamp(T, READ_DATA, subst=1))
        self.assertFalse(self.stamp(B, WRITE_ACK, subst=1))     # boiler echo of the gateway write
        self.assertFalse(self.stamp(B, UNKNOWN, subst=1))       # boiler without MsgID 27
        self.assertFalse(self.stamp(B, DATA_INVALID))
        # An A frame can be an OT-Direct cache answer (handleMasterModeSlaveFrame) that repeats an old
        # value, and an answer-override A never writes the canonical Toutside: never a stamp.
        self.assertFalse(self.stamp(A, READ_ACK, override=1))
        self.assertFalse(self.stamp(A, READ_ACK))
        self.assertFalse(self.stamp(P, READ_ACK))

    def test_other_msgid_does_not_stamp(self):
        self.assertFalse(self.stamp(R, WRITE_DATA, msgid=24))
        self.assertFalse(self.stamp(B, READ_ACK, msgid=28))


class SummaryNeverStamps(unittest.TestCase):
    """ADR-181: the PS=1 summary repeats stored values with no age, so it must not stamp MsgID 27."""

    def test_ps_summary_state_update_has_no_stamp(self):
        src = CORE_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        m = re.search(r"static void updatePSSummaryFloatState\(.*?\n}\n", src, re.S)
        self.assertIsNotNone(m, "updatePSSummaryFloatState not found")
        body = m.group(0)
        self.assertNotIn("iToutsideBoilerMs", body)
        self.assertNotIn("otFrameIsFreshOutsideTemp", body)

    def test_stamp_written_only_behind_the_predicate(self):
        src = CORE_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        writes = [m.start() for m in re.finditer(r"state\.otBus\.iToutsideBoilerMs\s*=\s*millis\(\)", src)]
        self.assertEqual(len(writes), 1, "expected exactly one stamp write in OTGW-Core.ino")
        before = src[max(0, writes[0] - 200):writes[0]]
        self.assertIn("if (otFrameIsFreshOutsideTemp(OTdata))", before)


if __name__ == "__main__":
    unittest.main()
