#!/usr/bin/env python3
"""TASK-1800: the OT-Direct PS=1 summary is byte for byte the PIC's.

Robert (2026-11-11): "OT-Direct can also be asked for a report with PS=1. That must always be identical to the
PIC's answers."

PIC ground truth (other-projects/otgw-6.6/gateway.asm):
  - PS= (PrintSummary :2341-2358): 'PS: 1' / 'PS: 0'; PS=1 asks for ONE summary line (SummaryRequest, cleared by
    SummaryDone :1656) and hides the T/B/R/A reports (HideReports, PrintMessage :1921-1922) until PS=0.
  - Store (StoreValue :3574-3609, tables :3530-3572): the raw data word of 34 Data-IDs, from a thermostat
    WRITE_DATA (HandleRequest :2384-2385), the gateway's own WRITE_DATA in standalone (StandAloneMsg :1806-1808)
    and a boiler READ_ACK (HandleResponse :2434-2436); Data-Invalid / Unknown-DataID are not stored (:3576-3577).
  - Print (StoreProcTable :1978-2012) with PrintStatus, PrintFloat, PrintSigned, PrintBytes, PrintLowByte,
    PrintShort and PrintSigShort (:2014-2135).

The PIC side is the Python oracle pic_line() below, written from those routines (with the citations), plus
literal expected fields that pin the oracle itself. The OT-Direct side is REAL code cut from
src/OTGW-firmware/OTDirect.ino: bridgeFrameToParser(), the PS=1 store/print block and emitSummaryLine(), the PS=
branch of handleOTDirectCommand(), synthesizeResponse() and the `if (otSummaryPending)` step of loopOTDirect().
The once-per-PS=1 rule is checked on the real gateway loop of tests/test_otd_gateway_alt_slots.py.

Old-vs-fix: OTGW_OTD_INO points at another copy of OTDirect.ino, e.g.
  git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
  OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_ps1_summary_pic.py
The old emitSummaryLine() reads the decoded state (OTcurrentSystemState); the harness fills it from the same
frames, so the old code is judged on its real output too.
Run: python tests/test_otd_ps1_summary_pic.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_otd_gateway_alt_slots as alt  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
OTD_INO = alt.OTD_INO
CORE_H = REPO / "src" / "OTGW-firmware" / "OTGW-Core.h"

# ---- PIC oracle -------------------------------------------------------------------------------------------
PIC_IDS = [0, 1, 6, 7, 8, 14, 15, 16, 17, 18, 19, 23, 24, 25, 26, 27, 28, 31, 33, 48, 49, 56, 57, 70, 71, 77,
           116, 117, 118, 119, 120, 121, 122, 123]                     # StoreFlagTable / StoreProcTable order
PIC_KIND = dict(zip(PIC_IDS, "S F S F F F B F F F F F F F F G F F H B B F F S L L U U U U U U U U".split()))


def pic_float(v):        # PrintFloat :2047-2059
    hi, lo = v >> 8, v & 0xFF
    if lo == 0xFF:
        hi, lo = (hi + 1) & 0xFF, 0
    prod = lo * 100
    frac = (prod >> 8) + (1 if prod & 0x80 else 0)
    return f"{hi}.{frac:02d}"


def pic_field(kind, v):
    if kind == "S":      # PrintStatus
        return f"{v >> 8:08b}/{v & 0xFF:08b}"
    if kind == "B":      # PrintBytes
        return f"{v >> 8}/{v & 0xFF}"
    if kind == "L":      # PrintLowByte
        return str(v & 0xFF)
    if kind == "U":      # PrintShort
        return str(v)
    if kind == "H":      # PrintSigShort
        return "-" + str((-v) & 0xFFFF) if v & 0x8000 else str(v)
    if kind == "G":      # PrintSigned -> PrintNegative + PrintFloat
        return "-" + pic_float((-v) & 0xFFFF) if v & 0x8000 else pic_float(v)
    return pic_float(v)  # "F"


def pic_line(store):
    return ",".join(pic_field(PIC_KIND[i], store.get(i, 0)) for i in PIC_IDS)


# ---- scenario: (prefix, type, id, data) in bus order ------------------------------------------------------
FRAMES = [
    ("B", 4, 0, 0x030A), ("T", 1, 1, 0x2880), ("B", 4, 6, 0x0301), ("T", 1, 14, 0x6400), ("B", 4, 15, 0x1E0A),
    ("T", 1, 16, 0x14C0), ("B", 4, 17, 0x3A1A), ("B", 4, 18, 0x0180), ("B", 4, 19, 0x00FF), ("T", 1, 24, 0x1499),
    ("B", 4, 25, 0x3CFF), ("B", 4, 26, 0x3200), ("B", 4, 27, 0xFB80), ("B", 4, 28, 0x2D40), ("B", 4, 33, 0xFFFB),
    ("B", 4, 48, 0x3C28), ("B", 4, 49, 0x5A14), ("T", 1, 56, 0x3700), ("T", 1, 57, 0x5000), ("B", 4, 70, 0x0012),
    ("B", 4, 71, 0x0037), ("B", 4, 77, 0x0028), ("B", 4, 35, 0x0F11), ("B", 4, 116, 0x3039), ("B", 4, 120, 0xFFFF),
    # not stored by the PIC:
    ("B", 6, 26, 0xDEAD),     # Data-Invalid
    ("B", 7, 18, 0x0000),     # Unknown-DataID
    ("T", 0, 25, 0x0000),     # a READ_DATA request
    ("A", 4, 17, 0x1111),     # the gateway's own answer to the thermostat
    ("R", 1, 1, 0x3C00),      # the gateway's own write while a thermostat is connected
    ("B", 4, 200, 0x1234),    # vendor Data-ID
]


EDGE_FRAMES = [("R", 1, 1, 0x3C00), ("B", 4, 27, 0x8000), ("B", 4, 25, 0xFFFF), ("B", 4, 33, 0x8000)]


def pic_store():
    st = {}
    for pre, typ, i, d in FRAMES:
        if (pre == "T" and typ == 1) or (pre == "B" and typ == 4):
            if i in PIC_KIND:
                st[i] = d
    return st


# ---- C++ harness ------------------------------------------------------------------------------------------
STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#define PROGMEM
#define PSTR(s) (s)
#define snprintf_P snprintf
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define OTDDebugTf(...) do {} while (0)
#define OTDDebugTln(...) do {} while (0)
static char* dtostrf(double v, int w, int p, char* out) { sprintf(out, "%*.*f", w, p, v); return out; }
static std::vector<std::string> g_bridge;   // port 25238 lines
static std::vector<std::string> g_parsed;   // processOT() lines
static void otDirectBridgeWriteLine(const char* l, size_t n) { g_bridge.push_back(std::string(l, n)); }
static void processOT(const char* l, size_t n) { g_parsed.push_back(std::string(l, n)); }
static void otDirectBridgeProcessStatus(const char* s) { otDirectBridgeWriteLine(s, 2); processOT(s, 2); }
enum OTFrameSrc { OTFRAME_SRC_OTDIRECT, OTFRAME_SRC_OTDIRECT_LOCAL };
static void enqueueOTFrame(const char*, size_t, bool, OTFrameSrc) {}
#define IS_LOOPBACK_MODE() (false)
static struct { struct { bool bThermostatConnected; } otd; } state = { { true } };
static bool otHideReports = false;
static bool otSummaryPending = false;
static bool otSummaryMode = false;          // old code only
@CORE_STRUCT@
"""

MAIN = r"""
@FILL_OLD@
static void prQuery(const char* value, const char* buf) {
  const char cmd0 = buf[0], cmd1 = buf[1];
  char rspBuf[16];
  (void)rspBuf;
  if (false) {}
  @PS_BRANCH@
}
static bool psCmd(const char* buf) { prQuery(buf + 3, buf); return true; }
static void loopStep() {
  @LOOP_STEP@
}
static void dump(const char* tag) {
  for (auto& l : g_bridge) printf("%s BRIDGE %s\n", tag, l.c_str());
  for (auto& l : g_parsed) printf("%s PARSED %s\n", tag, l.c_str());
  g_bridge.clear(); g_parsed.clear();
}
int main() {
  @FEED@
  fillOld();
  dump("FEED");
  psCmd("PS=1"); dump("CMD1");
  loopStep(); dump("LOOP1");
  loopStep(); dump("LOOP2");
  bridgeFrameToParser('T', 0x00000200UL); bridgeFrameToParser('B', 0x40000A00UL); dump("HIDDEN");
  psCmd("PS=1"); loopStep(); dump("AGAIN");
  psCmd("PS=0"); dump("CMD0");
  bridgeFrameToParser('T', 0x00000200UL); dump("SHOWN");
  psCmd("PS=2"); dump("BAD");
  psCmd("PS=10"); dump("BADLEN");
  // EDGE: no thermostat, so the gateway's own WRITE is stored (StandAloneMsg); 0x8000 and 0xFFFF edges
  state.otd.bThermostatConnected = false;
  @EDGE_FEED@
  psCmd("PS=1"); loopStep(); dump("EDGE");
  return 0;
}
"""


def frame_word(typ, i, d):
    f = (typ << 28) | (i << 16) | d
    if bin(f).count("1") % 2:
        f |= 0x80000000
    return f


def cut_block(src, regex):
    m = re.search(regex, src, re.M)
    if not m:
        raise AssertionError("not found: " + regex)
    i = src.index("{", m.start())
    depth = 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    raise AssertionError("unbalanced " + regex)


def build_source(src):
    fix = "otdPsNoteFrame" in src
    parts = []
    if fix:
        s = src.index("// PS=1 summary (TASK-1800)")
        s = src.rindex("// -----", 0, s)
        e = src.index("// ===========================================================================\n// TASK-184")
        ps_block = src[s:e]
    else:
        ps_block = cut_block(src, r"^static void emitSummaryLine\(\)")
    bridge = cut_block(src, r"^static void bridgeFrameToParser\(char prefix, unsigned long frame\)")
    if fix:
        parts.append("static void otdPsNoteFrame(char prefix, unsigned long frame);")
    parts.append(cut_block(src, r"^static inline bool checkBoolean\(const char\* value\)"))
    parts.append(cut_block(src, r"^static void synthesizeResponse\(char c0, char c1, const char\* value\)"))
    parts.append(cut_block(src, r"^static void synthesizeResponse\(const char\* cmd, const char\* value\)"))
    parts.append(bridge)
    parts.append(ps_block)
    ps_branch = cut_block(src, r"else if \(cmd0 == 'P' && cmd1 == 'S'\)")
    ps_branch = ps_branch.replace("return false;", "return;")
    loop_step = cut_block(src, r"^  if \(otSummaryPending\) \{")
    edge = "\n  ".join(f"bridgeFrameToParser('{p}', 0x{frame_word(t, i, d):08X}UL);" for p, t, i, d in EDGE_FRAMES)
    feed = "\n  ".join(f"bridgeFrameToParser('{p}', 0x{frame_word(t, i, d):08X}UL);" for p, t, i, d in FRAMES)
    core = ""
    fill = "static void fillOld() {}"
    if not fix:
        h = CORE_H.read_text(encoding="utf-8")
        e = h.index("} OTdataStruct;") + len("} OTdataStruct;")
        core = h[h.rindex("typedef struct", 0, e):e] + "\nstatic OTdataStruct OTcurrentSystemState;\n"
        st = pic_store()
        f88 = lambda v: (v - 0x10000 if v & 0x8000 else v) / 256.0
        sets = {0: ("Statusflags", None), 1: ("TSet", 1), 6: ("RBPflags", None), 14: ("MaxRelModLevelSetting", 1),
                15: ("MaxCapacityMinModLevel", None), 16: ("TrSet", 1), 17: ("RelModLevel", 1), 18: ("CHPressure", 1),
                19: ("DHWFlowRate", 1), 24: ("Tr", 1), 25: ("Tboiler", 1), 26: ("Tdhw", 1), 27: ("Toutside", 1),
                28: ("Tret", 1), 33: ("Texhaust", "s"), 48: ("TdhwSetUBTdhwSetLB", None),
                49: ("MaxTSetUBMaxTSetLB", None), 56: ("TdhwSet", 1), 57: ("MaxTSet", 1), 70: ("StatusVH", None),
                71: ("RelativeVentilation", None), 77: ("RelativeVentilation", None), 116: ("BurnerStarts", None),
                120: ("BurnerOperationHours", None)}
        lines = []
        for i, (name, kind) in sets.items():
            if i not in st:
                continue
            v = st[i]
            if kind == 1:
                lines.append(f"OTcurrentSystemState.{name} = {f88(v)}f;")
            elif kind == "s":
                lines.append(f"OTcurrentSystemState.{name} = (int16_t){v - 0x10000 if v & 0x8000 else v};")
            else:
                lines.append(f"OTcurrentSystemState.{name} = {v};")
        lines.append("OTcurrentSystemState.FanSpeed = 0x0F11;")   # the MsgID 35 frame of the scenario
        fill = "static void fillOld() {\n  " + "\n  ".join(lines) + "\n}"
    pre = STUBS.replace("@CORE_STRUCT@", core)
    main = (MAIN.replace("@FILL_OLD@", fill).replace("@PS_BRANCH@", ps_branch)
                .replace("@LOOP_STEP@", loop_step).replace("@FEED@", feed).replace("@EDGE_FEED@", edge))
    return pre + "\n".join(parts) + "\n" + main


def run_harness(ino):
    src = Path(ino).read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory() as d:
        cpp = Path(d) / "ps.cpp"
        cpp.write_text(build_source(src), encoding="utf-8")
        exe = compile_cpp(cpp, Path(d) / ("ps.exe" if os.name == "nt" else "ps"))
        if exe is None:
            return None
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
    res = {}
    for line in out.splitlines():
        tag, where, text = line.split(" ", 2)
        res.setdefault(tag, {"BRIDGE": [], "PARSED": []})[where].append(text)
    return res


# ---- once per PS=1 on the real gateway loop ---------------------------------------------------------------
def count_summaries_in_gateway_loop(ino):
    src = Path(ino).read_text(encoding="utf-8")
    enums, statics, types = alt.lib_and_types()
    pre = alt.PRE.replace("@LIB_ENUMS@", enums).replace("@LIB_STATICS@", statics).replace("@TYPES@", types)
    stub = "static void emitSummaryLine() {}\n"
    assert pre.count(stub) == 1
    pre = pre.replace(stub, "static int g_summaries = 0;\nstatic void emitSummaryLine() { g_summaries++; }\n")
    boiler = alt.MAIN[alt.MAIN.index("// ---- boiler model"):alt.MAIN.index("int main() {")]
    arm = "otSummaryPending = true; otHideReports = true;" + (" otSummaryMode = true;" if "otSummaryMode" in src else "")
    main = boiler + "int main() {\n  resetAll(); buildStream(0x02);\n  " + arm + \
        "\n  simulate(true, 60000);\n  printf(\"SUMMARIES %d\\n\", g_summaries);\n  return 0;\n}\n"
    with tempfile.TemporaryDirectory() as d:
        cpp = Path(d) / "t.cpp"
        cpp.write_text(pre + alt.slices(src) + main, encoding="utf-8")
        exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
        if exe is None:
            return None
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
    return int(re.search(r"^SUMMARIES (\d+)$", out, re.M).group(1))


class PS1SummaryIsThePICs(unittest.TestCase):
    res = None

    @classmethod
    def setUpClass(cls):
        cls.res = run_harness(OTD_INO)
        if cls.res is None:
            raise unittest.SkipTest("no host compiler")
        cls.golden = pic_line(pic_store())
        for tag, v in cls.res.items():
            for where in ("BRIDGE", "PARSED"):
                for t in v[where]:
                    print(f"{tag} {where} {t}")
        print("GOLDEN", cls.golden)

    def summary_lines(self, tag, where):
        return [l for l in self.res.get(tag, {}).get(where, []) if "," in l]

    def test_00_oracle_pins(self):
        # literal PIC renderings, worked by hand from the routines, so the oracle cannot drift
        f = dict(zip(PIC_IDS, self.golden.split(",")))
        self.assertEqual(f[0], "00000011/00001010")
        self.assertEqual(f[15], "30/10")
        self.assertEqual(f[17], "58.10")
        self.assertEqual(f[19], "1.00")      # 0x00FF rounds up
        self.assertEqual(f[24], "20.60")     # 0x99 -> .597 rounds to .60
        self.assertEqual(f[25], "61.00")
        self.assertEqual(f[26], "50.00")     # the later Data-Invalid is not stored
        self.assertEqual(f[27], "-4.50")     # no '+'
        self.assertEqual(f[33], "-5")
        self.assertEqual(f[71], "55")
        self.assertEqual(f[77], "40")
        self.assertEqual(f[1], "40.50")      # the gateway's own R write with a thermostat is not stored
        self.assertEqual(f[120], "65535")
        self.assertEqual(f[7], "0.00")       # never seen
        self.assertEqual(self.golden.count(","), 33)

    def test_A1_ps1_answer_then_one_pic_line_on_25238_and_parser(self):
        self.assertEqual(self.res["CMD1"]["BRIDGE"], ["PS: 1"])
        self.assertEqual(self.summary_lines("LOOP1", "BRIDGE"), [self.golden], "port 25238 line differs from the PIC")
        self.assertEqual(self.summary_lines("LOOP1", "PARSED"), [self.golden], "parser line differs from the PIC")

    def test_A2_one_line_per_ps1(self):
        self.assertEqual(self.summary_lines("LOOP2", "BRIDGE") + self.summary_lines("LOOP2", "PARSED"), [])
        # values keep being stored while the reports are hidden (StoreValue runs in HandleResponse): the
        # hidden boiler READ_ACK MsgID 0 = 0x0A00 is in the second line
        st = pic_store(); st[0] = 0x0A00
        self.assertEqual(self.summary_lines("AGAIN", "BRIDGE"), [pic_line(st)], "a second PS=1 gives a second line")

    def test_A3_reports_hidden_on_25238_while_ps1_and_back_after_ps0(self):
        self.assertEqual(self.res.get("HIDDEN", {}).get("BRIDGE", []), [])
        self.assertEqual(self.res["CMD0"]["BRIDGE"], ["PS: 0"])
        self.assertEqual(self.res["SHOWN"]["BRIDGE"], ["T00000200"])

    def test_A4_bad_value(self):
        self.assertEqual(self.res["BAD"]["BRIDGE"], ["PS: BV"])   # SerialCommand prints "PS: " first

    def test_A4b_syntax_error(self):
        self.assertEqual(self.res["BADLEN"]["BRIDGE"], ["PS: SE"])   # PrintSummary: rxpointer must be 4

    def test_A7_standalone_own_write_and_edges(self):
        st = pic_store()
        st[0] = 0x0A00                                       # the hidden B frame of the HIDDEN step
        st.update({1: 0x3C00, 27: 0x8000, 25: 0xFFFF, 33: 0x8000})
        line = pic_line(st)
        f = dict(zip(PIC_IDS, line.split(",")))
        self.assertEqual((f[1], f[27], f[25], f[33]), ("60.00", "-128.00", "0.00", "-32768"))   # PIC wraps 255+1
        self.assertEqual(self.summary_lines("EDGE", "BRIDGE"), [line])

    def test_A5_processPSSummary_field_count(self):
        # OTGW-Core.ino processPSSummary(): only 24 or 33 commas are taken as a summary line
        line = self.summary_lines("LOOP1", "PARSED")[0]
        self.assertEqual(line.count(","), 33)

    def test_A6_no_line_after_every_msgid0_in_the_gateway_loop(self):
        n = count_summaries_in_gateway_loop(OTD_INO)
        if n is None:
            self.skipTest("no host compiler")
        self.assertEqual(n, 1, "PS=1 must give one summary line, not one after every MsgID 0")


if __name__ == "__main__":
    unittest.main(verbosity=2)
