#!/usr/bin/env python3
"""TASK-1859: OT-Direct numeric commands parse and print like the PIC 6.8.

PIC ground truth (OTGW-other-projects/otgw-6.8, repo 770aa45):
  - gateway.asm GetFloat :5118-5185, GetPosFloat :5114-5117, GetPercent :5099-5112, GetDecimal :5045-5069,
    Divide100 :5211-5225 (parser); PrintSigned :2043-2064 (the CommandFloat reply);
  - the expected bytes and replies come from the PIC's own test scripts, tests/*.stc:
    control-1 CS=49.72 -> R100131B9 / "CS: 49.72"; control-3 CS=54.3 -> R1001364D; control-4 CS=5 -> R10010500;
    standalone-7 CS=42.88 -> R90012AE2 / "CS: 42.88", CS=0 -> "CS: 0.00"; command-4 "CS=" -> SE;
    command-2 C2=45.6 -> "C2: 45.60"; control-5 BS=24.6 -> R1010189A / "BS: 24.60", BS=0 -> "BS: 0.00";
    heatpump-1 RT=25.41 -> R10181969; heatpump-2 CL=94.38 -> R10075E62;
    command-6 OT=9.999 -> 10.00, OT=-9.999 -> -10.00, OT=-0.004 -> 0.00; command-7 OT=-1.4 -> R101BFE9A;
    sensor-6 OT=7.6 -> A401B079A.

The code under test is REAL: the "// --- otdPicFloat" block and the CS=, C2=, BS=, CL= and RT= branches of
handleOTDirectCommand() are cut from src/OTGW-firmware/OTDirect.ino and compiled with doubles that record what
the branch enqueues, sets and answers.

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_pic_float.py
Run: python tests/test_otd_pic_float.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")


def braced(src, start_pat):
    m = re.search(start_pat, src, re.M)
    if not m:
        raise AssertionError("not found: " + start_pat)
    i, depth = src.index("{", m.start()), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    raise AssertionError("unbalanced: " + start_pat)


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cctype>
#define PSTR(s) (s)
#define snprintf_P snprintf
#define F(s) (s)
#define OTDDebugTln(...) do {} while (0)
#define OTDDebugTf(...) do {} while (0)
static void dtostrf(double v, int, int prec, char* out) { sprintf(out, "%.*f", prec, v); }
static char g_rsp[48], g_status[8];
static int g_enq = -1, g_clear = -1, g_set = -1;
static uint16_t g_val = 0;
static void synthesizeResponse(const char*, const char* rsp) { snprintf(g_rsp, sizeof g_rsp, "%s", rsp); }
static void otDirectBridgeProcessStatus(const char* s) { snprintf(g_status, sizeof g_status, "%s", s); }
static bool enqueueWriteCommand(uint8_t id, uint16_t v, const char*) { g_enq = id; g_val = v; return true; }
static void clearWriteOverride(uint8_t id) { g_clear = id; }
static void setOverride(uint8_t id, uint16_t v) { g_set = id; g_val = v; }
static void updateWriteCache(uint8_t, uint16_t) {}
static uint32_t otdCsc2ExpiryStamp(uint16_t) { return 1; }
static uint32_t otCSLastCommandMs = 0, otC2LastCommandMs = 0;
static float otFakeRoomSetpoint = 0.0f;
// TT=/TC=/SB= doubles
enum OTRemoteOverrideMode { OT_OVERRIDE_TEMPORARY = 1, OT_OVERRIDE_CONSTANT = 2 };
static int g_rmode = 0; static float g_rval = 0; static int g_rclear = 0;
static void setRemoteOverride(OTRemoteOverrideMode m, float c) { g_rmode = (int)m; g_rval = c; }
static void clearRemoteOverride() { g_rclear++; }
static float otSetbackTemp = 16.0f;
struct { struct { float fSetbackTemp = 16.0f; float fDhwSetpoint = 0; float fMaxChSetpoint = 0; } otd; } settings;
static void otdPersistFloat(float& slot, float v) { slot = v; }
static float constrain(double v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : (float)v); }   // the old SB= clamp
// SW=/SH=/OT= doubles: the boiler cache for the MsgID 48/49 bounds, the PS=1 outside value, the system state
static uint16_t otBoilerCache[256]; static bool otBoilerCacheValid[256];
static float g_dhwPersist = -1, g_chPersist = -1;
static uint16_t g_psOutside = 0;
static void otdPsStoreOutside(uint16_t v) { g_psOutside = v; }
struct { float Toutside = 0; } OTcurrentSystemState;
static inline uint16_t otdUnused() { return 0; }
"""

MAIN = r"""
static bool cmd(const char* buf, const char* value) {
  const char cmd0 = buf[0], cmd1 = buf[1];
  char rspBuf[16];
  g_rsp[0] = 0; g_status[0] = 0; g_enq = g_clear = g_set = -1; g_val = 0; g_rmode = 0; g_rval = 0; g_rclear = 0;
  if (false) {}
  else if (cmd0 == 'X') {   // test only: X8=<hex> / X9=<hex> puts a boiler MsgID 48 / 49 reply in the cache
    const uint8_t id = cmd1 == '8' ? 48 : 49;
    otBoilerCache[id] = (uint16_t)strtoul(value, nullptr, 16); otBoilerCacheValid[id] = true;
  }
  @BRANCHES@
  return true;
}
int main(int argc, char** argv) {
  if (argc > 1 && !strcmp(argv[1], "cmd")) {
    for (int a = 2; a + 1 < argc; a += 2) {
      cmd(argv[a], argv[a + 1]);
      printf("%s=%s|rsp=%s|st=%s|enq=%d|set=%d|clr=%d|val=%04X|rmode=%d|rval=%.4f|rclr=%d|sb=%.4f|dhw=%.4f|ch=%.4f"
             "|ps=%04X|tout=%.4f\n", argv[a], argv[a + 1], g_rsp, g_status, g_enq, g_set, g_clr_dummy(), g_val, g_rmode,
             g_rval, g_rclear, otSetbackTemp, settings.otd.fDhwSetpoint, settings.otd.fMaxChSetpoint, g_psOutside,
             OTcurrentSystemState.Toutside);
    }
    return 0;
  }
#ifdef HAVE_PICFLOAT
  if (argc > 1 && !strcmp(argv[1], "parse")) {   // parse <kind> <value>...
    const uint8_t k = argv[2][0] == 'F' ? OTD_PIC_FLOAT : argv[2][0] == 'P' ? OTD_PIC_POSFLOAT : OTD_PIC_PERCENT;
    for (int a = 3; a < argc; a++) {
      uint16_t v = 0; bool pe = false;
      const char* e = otdPicFloatF88(argv[a], k, v, &pe);
      if (e) printf("%s=%s%s\n", argv[a], e, pe ? "" : "!"); else printf("%s=%04X\n", argv[a], v);
    }
    return 0;
  }
  if (argc > 1 && !strcmp(argv[1], "print")) {
    for (int a = 2; a < argc; a++) {
      char b[16]; otdPicPrintF88((uint16_t)strtoul(argv[a], nullptr, 16), b, sizeof b); printf("%s=%s\n", argv[a], b);
    }
    return 0;
  }
#endif
  return 2;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    blk = re.search(r"// --- otdPicFloat begin\n(.*?)// --- otdPicFloat end", SRC, re.S)
    old = re.search(r"// --- otdPicPercentF88 begin\n(.*?)// --- otdPicPercentF88 end", SRC, re.S)
    f88 = braced(SRC, r"^static inline uint16_t floatToF88\(float celsius\) \{")
    helpers = (("#define HAVE_PICFLOAT 1\n" + blk.group(1)) if blk else (old.group(1) if old else ""))
    if "static bool otdPicInBounds(" in SRC:   # the real SW=/SH= range check
        helpers += "\n" + braced(SRC, r"^static bool otdPicInBounds\(")
    branches = "\n  ".join(braced(SRC, p) for p in (
        r"if \(cmd0 == 'C' && cmd1 == 'S'\) \{",
        r"else if \(cmd0 == 'B' && cmd1 == 'S'\) \{",
        r"else if \(cmd0 == 'R' && cmd1 == 'T'\) \{",
        r"else if \(cmd0 == 'C' && cmd1 == '2'\) \{",
        r"else if \(cmd0 == 'C' && cmd1 == 'L'\) \{",
        r"else if \(cmd0 == 'T' && cmd1 == 'C'\) \{",
        r"else if \(cmd0 == 'T' && cmd1 == 'T'\) \{",
        r"else if \(cmd0 == 'S' && cmd1 == 'B'\) \{",
        r"else if \(cmd0 == 'C' && cmd1 == 'C'\) \{",
        r"else if \(cmd0 == 'S' && cmd1 == 'W'\) \{",
        r"else if \(cmd0 == 'S' && cmd1 == 'H'\) \{",
        r"else if \(cmd0 == 'O' && cmd1 == 'T'\) \{")).replace("if (cmd0 == 'C' && cmd1 == 'S')",
                                                             "else if (cmd0 == 'C' && cmd1 == 'S')", 1)
    main = MAIN.replace("@BRANCHES@", branches).replace("g_clr_dummy()", "g_clear")
    _tmp = Path(tempfile.mkdtemp(prefix="otd_pf_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(PRE + f88 + "\n" + helpers + main, encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(*args):
    p = subprocess.run([str(_exe)] + list(args), capture_output=True, text=True)
    return p.returncode, p.stdout.splitlines()


def cmds(*pairs):
    args = ["cmd"]
    for c, v in pairs:
        args += [c, v]
    rc, out = run(*args)
    rows = {}
    for line in out:
        head, *fields = line.split("|")
        rows[head] = dict(f.split("=", 1) for f in fields)
    return rows


class Parser(unittest.TestCase):
    def table(self, kind, cases):
        rc, out = run("parse", kind, *[c for c, _ in cases])
        self.assertEqual(rc, 0, "otdPicFloatF88() missing (old code)")
        self.assertEqual(out, [f"{c}={w}" for c, w in cases])

    def test_getfloat_signed_like_the_pic(self):
        self.table("F", [("9.999", "0A00"), ("-9.999", "F600"), ("-0.004", "0000"), ("-1.4", "FE9A"),
                         ("7.6", "079A"), ("-1.00", "FF00"), ("-1.5", "FE80"), ("-1.995", "FE00"),
                         ("25.41", "1969"), ("0.02", "0006"), ("+5", "0500"), ("55.7", "37B4"),
                         ("1.2.3", "SE"), ("", "SE"), ("x", "SE"), ("5x", "SE"), (".5", "SE"), ("256", "OR"),
                         ("1.23456789", "013B")])

    def test_getposfloat_refuses_a_minus(self):
        self.table("P", [("-1", "SE"), ("16.1", "101A"), ("16.2", "1034"), ("18.23", "123B")])

    def test_getpercent_range_error_is_not_a_parse_error(self):
        # '!' marks an error that is not a parse error (GetPercent's own OR, DC clear)
        self.table("C", [("100", "6400"), ("100.01", "OR!"), ("101", "OR!"), ("256", "OR"), ("-1", "SE"),
                         ("49.72", "31B9"), ("94.38", "5E62")])

    def test_print_signed_like_the_pic(self):
        rc, out = run("print", "1969", "0020", "FE9A", "F600", "00FF", "0000", "0A00", "8000", "31B9")
        self.assertEqual(rc, 0, "otdPicPrintF88() missing (old code)")
        self.assertEqual(out, ["1969=25.41", "0020=0.13", "FE9A=-1.40", "F600=-10.00", "00FF=1.00", "0000=0.00",
                               "0A00=10.00", "8000=-128.00", "31B9=49.72"])


class PercentCommands(unittest.TestCase):
    def test_CS_bytes_and_reply_from_the_pic_scripts(self):
        r = cmds(("CS", "49.72"), ("CS", "54.3"), ("CS", "5"), ("CS", "42.88"), ("CS", "0"), ("CS", "0.02"))
        self.assertEqual((r["CS=49.72"]["val"], r["CS=49.72"]["rsp"]), ("31B9", "49.72"))
        self.assertEqual(r["CS=54.3"]["val"], "364D")
        self.assertEqual((r["CS=5"]["val"], r["CS=5"]["rsp"]), ("0500", "5.00"))
        self.assertEqual((r["CS=42.88"]["val"], r["CS=42.88"]["rsp"]), ("2AE2", "42.88"))
        self.assertEqual((r["CS=0"]["clr"], r["CS=0"]["enq"], r["CS=0"]["rsp"]), ("1", "-1", "0.00"))
        self.assertEqual((r["CS=0.02"]["val"], r["CS=0.02"]["rsp"]), ("0006", "0.02"))

    def test_CS_refusals(self):
        r = cmds(("CS", ""), ("CS", "-5"), ("CS", "100.01"), ("CS", "1.x"))
        for k, st in (("CS=", "SE"), ("CS=-5", "SE"), ("CS=100.01", "OR"), ("CS=1.x", "SE")):
            self.assertEqual((r[k]["st"], r[k]["enq"], r[k]["clr"]), (st, "-1", "-1"), k)

    def test_C2_BS_RT(self):
        r = cmds(("C2", "45.6"), ("BS", "24.6"), ("BS", "0"), ("RT", "25.41"), ("RT", "25.4"))
        self.assertEqual((r["C2=45.6"]["enq"], r["C2=45.6"]["val"], r["C2=45.6"]["rsp"]), ("8", "2D9A", "45.60"))
        self.assertEqual((r["BS=24.6"]["set"], r["BS=24.6"]["val"], r["BS=24.6"]["rsp"]), ("16", "189A", "24.60"))
        self.assertEqual((r["BS=0"]["clr"], r["BS=0"]["rsp"]), ("16", "0.00"))
        self.assertEqual((r["RT=25.41"]["val"], r["RT=25.41"]["rsp"]), ("1969", "25.41"))
        # one fraction digit is tenths (Multiply10 :5144-5145); the TASK-1842 parser read it as hundredths
        self.assertEqual((r["RT=25.4"]["val"], r["RT=25.4"]["rsp"]), ("1967", "25.40"))

    def test_CL_clear_on_parse_error_OR_above_100(self):
        r = cmds(("CL", "94.38"), ("CL", "x"), ("CL", "101"), ("CL", "0"))
        self.assertEqual((r["CL=94.38"]["enq"], r["CL=94.38"]["val"], r["CL=94.38"]["rsp"]), ("7", "5E62", "94.38"))
        self.assertEqual((r["CL=x"]["clr"], r["CL=x"]["rsp"]), ("7", "-"))
        self.assertEqual((r["CL=101"]["st"], r["CL=101"]["enq"], r["CL=101"]["clr"]), ("OR", "-1", "-1"))
        self.assertEqual((r["CL=0"]["enq"], r["CL=0"]["val"], r["CL=0"]["rsp"]), ("7", "0000", "0.00"))


    def test_CC_like_its_sibling_CL(self):
        r = cmds(("CC", "94.38"), ("CC", "101"), ("CC", "x"))
        self.assertEqual((r["CC=94.38"]["enq"], r["CC=94.38"]["val"], r["CC=94.38"]["rsp"]), ("7", "5E62", "94.38"))
        self.assertEqual((r["CC=101"]["st"], r["CC=x"]["st"]), ("OR", "SE"))


class PosFloatCommands(unittest.TestCase):
    def test_TT_TC_values_from_the_pic_scripts(self):
        r = cmds(("TT", "16.1"), ("TC", "16.2"), ("TT", "0"), ("TC", "20.555"))
        self.assertEqual((r["TT=16.1"]["rmode"], r["TT=16.1"]["rval"], r["TT=16.1"]["rsp"]), ("1", "16.1016", "16.10"))  # 0x101A
        self.assertEqual((r["TC=16.2"]["rmode"], r["TC=16.2"]["rval"], r["TC=16.2"]["rsp"]), ("2", "16.2031", "16.20"))  # 0x1034
        self.assertEqual((r["TT=0"]["rclr"], r["TT=0"]["rsp"]), ("1", "0.00"))
        self.assertEqual(r["TC=20.555"]["rsp"], "20.56")

    def test_TT_TC_refuse_a_minus_and_garbage(self):
        r = cmds(("TT", "-5"), ("TC", "-1.5"), ("TT", ""), ("TC", "abc"))
        for k in ("TT=-5", "TC=-1.5", "TT=", "TC=abc"):
            self.assertEqual((r[k]["st"], r[k]["rmode"], r[k]["rclr"]), ("SE", "0", "0"), k)

    def test_SB_like_set_setback(self):
        r = cmds(("SB", "18.23"), ("SB", "35"), ("SB", "0"), ("SB", "-2"), ("SB", "0.5"))
        self.assertEqual((r["SB=18.23"]["sb"], r["SB=18.23"]["rsp"]), ("18.2305", "18.23"))   # 0x123B, gpio-5.stc
        self.assertEqual((r["SB=35"]["sb"], r["SB=35"]["rsp"]), ("35.0000", "35.00"))         # no clamp to 30
        self.assertEqual((r["SB=0.5"]["sb"], r["SB=0.5"]["rsp"]), ("0.5000", "0.50"))         # no clamp to 1
        self.assertEqual(r["SB=0"]["st"], "SE")                                                # zero is SE
        self.assertEqual(r["SB=-2"]["st"], "SE")


class FloatCommands(unittest.TestCase):
    def test_SW_SH_value_bounds_and_reset(self):
        r = cmds(("SW", "55.7"), ("SW", "80"), ("SW", "80.01"), ("SW", "19.99"), ("SW", "-5"), ("SW", "0"),
                 ("SH", "55"), ("SH", "90"), ("SH", "91"), ("SH", "9"))
        self.assertEqual((r["SW=55.7"]["enq"], r["SW=55.7"]["val"], r["SW=55.7"]["rsp"], r["SW=55.7"]["dhw"]),
                         ("56", "37B4", "55.70", "55.7031"))            # dhw-7.stc R903837B4
        self.assertEqual(r["SW=80"]["val"], "5000")                      # the upper bound itself is allowed
        for k in ("SW=80.01", "SW=19.99", "SW=-5", "SH=91", "SH=9"):
            self.assertEqual((r[k]["st"], r[k]["enq"]), ("BV", "-1"), k)
        self.assertEqual((r["SW=0"]["clr"], r["SW=0"]["rsp"]), ("56", "0.00"))
        self.assertEqual((r["SH=55"]["enq"], r["SH=55"]["val"]), ("57", "3700"))
        self.assertEqual(r["SH=90"]["val"], "5A00")

    def test_SW_bounds_from_the_boiler(self):
        r = cmds(("X8", "3C28"), ("SW", "65"), ("SW", "60"), ("SW", "39"), ("SW", "40"))   # boiler: 40..60
        self.assertEqual((r["SW=65"]["st"], r["SW=39"]["st"]), ("BV", "BV"))
        self.assertEqual((r["SW=60"]["val"], r["SW=40"]["val"]), ("3C00", "2800"))

    def test_OT_values_from_the_pic_scripts(self):
        r = cmds(("OT", "9.999"), ("OT", "-9.999"), ("OT", "-0.004"), ("OT", "-1.4"), ("OT", "7.6"))
        self.assertEqual((r["OT=9.999"]["val"], r["OT=9.999"]["rsp"]), ("0A00", "10.00"))
        self.assertEqual((r["OT=-9.999"]["val"], r["OT=-9.999"]["rsp"]), ("F600", "-10.00"))
        self.assertEqual((r["OT=-0.004"]["val"], r["OT=-0.004"]["rsp"]), ("0000", "0.00"))
        self.assertEqual((r["OT=-1.4"]["val"], r["OT=-1.4"]["ps"], r["OT=-1.4"]["rsp"]), ("FE9A", "FE9A", "-1.40"))
        self.assertEqual((r["OT=7.6"]["val"], r["OT=7.6"]["tout"]), ("079A", "7.6016"))

    def test_OT_clears_like_clr_outside_t(self):
        r = cmds(("OT", "-"), ("OT", "x"), ("OT", "64"), ("OT", "99"), ("OT", "63.99"), ("OT", "200"))
        for k in ("OT=-", "OT=x", "OT=64", "OT=99"):
            self.assertEqual((r[k]["clr"], r[k]["enq"], r[k]["rsp"]), ("27", "-1", "-"), k)
        self.assertEqual(r["OT=63.99"]["val"], "3FFE")
        self.assertEqual((r["OT=200"]["val"], r["OT=200"]["rsp"]), ("C800", "-56.00"))   # bit 7 set: stored, as the PIC


if __name__ == "__main__":
    unittest.main(verbosity=2)
