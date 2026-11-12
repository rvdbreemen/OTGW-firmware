#!/usr/bin/env python3
"""TASK-1846 (PIC 6.8 parity audit N12): after OT=, the PS=1 summary shows that value in the MsgID 27 field.

PIC (other-projects/otgw-6.8/gateway.asm): SetOutsideT :4256-4266 calls StoreOutTemp :4276-4287 for a valid value,
which writes the f8.8 value to outside1/outside2 AND to valuestorage+30 (outsideval1, :568-569), the summary slot
of MsgID 27 (index 15 of the 34 summary Data-IDs). OT=- only clears the OutsideTemp flag (ClrOutsideT :4262): the
summary slot keeps its last value. The PIC does this with or without a thermostat.

OT-Direct filled the store only from bus frames (otdPsNoteFrame), so with a thermostat connected an OT=7.5 never
reached field 16 of the line. The real code is cut from src/OTGW-firmware/OTDirect.ino: the PS=1 store/print block,
bridgeFrameToParser(), synthesizeResponse(), the PS= and OT= branches of handleOTDirectCommand() and the
otSummaryPending step of loopOTDirect(), on the stubs of tests/test_otd_ps1_summary_pic.py. enqueueWriteCommand()
is a stub that accepts the write. OTGW_OTD_INO points at another copy for old-vs-fix.
Run: python tests/test_otd_ps1_outside_temp.py
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
import test_otd_ps1_summary_pic as ps  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))

EXTRA = r"""
static struct { float Toutside; } OTcurrentSystemState;
static bool clearWriteOverride(uint8_t) { return true; }
static int g_writes27 = 0;
static bool enqueueWriteCommand(uint8_t id, uint16_t, const char*) { if (id == 27) g_writes27++; return true; }
"""

MAIN = r"""
static void cmd(const char* buf) {
  const char cmd0 = buf[0], cmd1 = buf[1];
  const char* value = buf + 3;
  char rspBuf[16];
  (void)rspBuf;
  if (false) {}
  @PS_BRANCH@
  @OT_BRANCH@
}
static void loopStep() {
  @LOOP_STEP@
}
static void summary(const char* tag) {
  g_bridge.clear(); g_parsed.clear();
  cmd("PS=1"); loopStep();
  for (auto& l : g_bridge) if (l.find(',') != std::string::npos) printf("%s %s\n", tag, l.c_str());
  cmd("PS=0"); g_bridge.clear(); g_parsed.clear();
}
int main(int argc, char** argv) {
  if (argv[1][0] == 's') {   // AC2: the frames of the PIC's own tests/summary-1.stc with the replies of tests/boiler.stc
    state.otd.bThermostatConnected = true;
    @STC_FEED@
    summary("STC");
    cmd("OT=7.5"); summary("STCOT");
    return 0;
  }
  state.otd.bThermostatConnected = argv[1][0] == '1';
  summary("BOOT");
  cmd("OT=7.5"); summary("OT75");
  cmd("OT=-3.25"); summary("OTNEG");
  cmd("OT=-"); summary("OTCLR");
  // a boiler READ_ACK for MsgID 27 still overwrites the slot, as on the PIC (HandleResponse StoreValue)
  bridgeFrameToParser('B', 0x401B0C80UL);   // 12.50
  summary("BOILER");
  printf("WRITES27 %d\n", g_writes27);
  return 0;
}
"""


PIC_TESTS = REPO / "other-projects" / "otgw-6.8" / "tests"


def stc_values(name, key):
    return [int(m.group(1), 16) for m in re.finditer(key + r"\s*=\s*0x([0-9A-Fa-f]{8})", (PIC_TESTS / name).read_text())]


def stc_expected():
    m = re.search(r'check \{\\S\+\} "([^"]+)"', (PIC_TESTS / "summary-1.stc").read_text())
    return m.group(1).replace("\\x2c", ",")


def stc_feed():
    # boiler.stc holds one reply word per DataID. Its type does not always match the request: the thermostat READs 56
    # and 57 (0x80380000, 0x80390000) while the table words are Write-Ack (0xD0383C00, 0xD0393200), and summary-1.stc
    # still expects 60.00 and 50.00, which the PIC stores only from a Read-Ack (HandleResponse :2438-2439, btfss byte1,4).
    # So the simulator is read here as taking the data word per DataID and answering a READ with Read-Ack and a WRITE
    # with Write-Ack. That reading is an assumption about the simulator, pinned by the expected line itself.
    replies = {(v >> 16) & 0xFF: v for v in stc_values("boiler.stc", "Boiler.response")}
    calls = []
    for req in stc_values("summary-1.stc", "Thermostat.tx"):
        calls.append(f"bridgeFrameToParser('T', 0x{req:08X}UL);")
        rsp = replies.get((req >> 16) & 0xFF)
        if rsp is not None:
            ack = 4 if ((req >> 28) & 0x7) == 0 else 5
            rsp = (ack << 28) | (rsp & 0x00FFFFFF)
            if bin(rsp).count("1") % 2:
                rsp |= 0x80000000
            calls.append(f"bridgeFrameToParser('B', 0x{rsp:08X}UL);")
    return "\n    ".join(calls)


def build(src):
    parts = []
    s = src.index("// PS=1 summary (TASK-1800)")
    s = src.rindex("// -----", 0, s)
    e = src.index("// ===========================================================================\n// TASK-184")
    parts.append("static void otdPsNoteFrame(char prefix, unsigned long frame);")
    parts.append(ps.cut_block(src, r"^static inline uint16_t floatToF88\(float celsius\)"))
    parts.append(ps.cut_block(src, r"^static inline bool checkBoolean\(const char\* value\)"))
    parts.append(ps.cut_block(src, r"^static void synthesizeResponse\(char c0, char c1, const char\* value\)"))
    parts.append(ps.cut_block(src, r"^static void synthesizeResponse\(const char\* cmd, const char\* value\)"))
    parts.append(ps.cut_block(src, r"^static void bridgeFrameToParser\(char prefix, unsigned long frame\)"))
    parts.append(src[s:e])
    ps_branch = ps.cut_block(src, r"else if \(cmd0 == 'P' && cmd1 == 'S'\)").replace("return false;", "return;")
    ot_branch = ps.cut_block(src, r"else if \(cmd0 == 'O' && cmd1 == 'T'\)").replace("return false;", "return;")
    loop_step = ps.cut_block(src, r"^  if \(otSummaryPending\) \{")
    pre = ps.STUBS.replace("@CORE_STRUCT@", EXTRA)
    main = MAIN.replace("@PS_BRANCH@", ps_branch).replace("@OT_BRANCH@", ot_branch).replace("@LOOP_STEP@", loop_step)
    main = main.replace("@STC_FEED@", stc_feed())
    return pre + "\n".join(parts) + "\n" + main


class OutsideTempInSummary(unittest.TestCase):
    exe = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(build(OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_with(self, thermostat):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        arg = thermostat if isinstance(thermostat, str) else ("1" if thermostat else "0")
        out = subprocess.run([str(self.exe), arg], capture_output=True, text=True, check=True).stdout
        res = {}
        for line in out.splitlines():
            tag, text = line.split(" ", 1)
            res[tag] = text
        return res

    @staticmethod
    def field16(line):
        return line.split(",")[15]   # MsgID 27 is the 16th of the 34 summary fields

    def test_ac1_thermostat_connected_ot_7_5_shows_in_field_16(self):
        r = self.run_with(True)
        self.assertEqual(self.field16(r["BOOT"]), "0.00")
        self.assertEqual(self.field16(r["OT75"]), "7.50", r["OT75"])
        self.assertEqual(r["WRITES27"], "2", "OT= still queues the MsgID 27 write as before")

    def test_negative_value_signed(self):
        self.assertEqual(self.field16(self.run_with(True)["OTNEG"]), "-3.25")

    def test_ot_clear_keeps_the_slot_like_the_pic(self):
        self.assertEqual(self.field16(self.run_with(True)["OTCLR"]), "-3.25")

    def test_boiler_read_ack_still_overwrites(self):
        self.assertEqual(self.field16(self.run_with(True)["BOILER"]), "12.50")

    def test_no_thermostat_same(self):
        self.assertEqual(self.field16(self.run_with(False)["OT75"]), "7.50")

    def test_ac2_pic_summary_1_stc_line_byte_identical(self):
        r = self.run_with("s")
        self.assertEqual(r["STC"], stc_expected())
        exp = stc_expected().split(",")
        exp[15] = "7.50"
        self.assertEqual(r["STCOT"], ",".join(exp), "after OT=7.5 only the MsgID 27 field changes")

    def test_only_field_16_changes(self):
        r = self.run_with(True)
        a, b = r["BOOT"].split(","), r["OT75"].split(",")
        self.assertEqual(len(a), 34)
        self.assertEqual([i for i in range(34) if a[i] != b[i]], [15])


if __name__ == "__main__":
    unittest.main(verbosity=2)
