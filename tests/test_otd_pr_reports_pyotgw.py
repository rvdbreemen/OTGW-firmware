#!/usr/bin/env python3
"""TASK-1797: OT-Direct PR=A and PR=T replies parse in pyotgw (HA opentherm_gw) and OTmonitor, like the PIC.

PIC ground truth (other-projects/otgw-6.6/gateway.asm):
  - PR=A prints GreetingStr "OpenTherm Gateway ", version (gateway.asm:5434-5435), e.g. "PR: A=OpenTherm Gateway 6.6";
  - PR=T prints two booleans without a separator, ignore-transitions then override-high-byte (PrintTweaks,
    gateway.asm:2278-2285), e.g. "PR: T=10".
Consumers:
  - pyotgw 2.2.3 get_reports() evaluates int(ver[18]) outside any try/except (pyotgw.py:177-183) and
    int(tweaks[0]), int(tweaks[1]) (pyotgw.py:224-230). A letter at index 18 or a '/' in the tweaks raises
    ValueError, and HA's config flow and entry setup fail;
  - OTmonitor matches "PR: T=[01][01]" (otmonitor.tcl:628-634) and takes the third word after "OpenTherm Gateway "
    as the firmware version it gates features on (otmonitor.tcl:552-562, gui.tcl:1080 needs 6.2).
The banner is "OpenTherm Gateway <version> OTGW32" (Robert, 2026-11-12): the version right after
"OpenTherm Gateway " for pyotgw and OTmonitor, then the board name, a deliberate difference from the PIC.

The code under test is REAL: the whole `else if (cmd0 == 'P' && cmd1 == 'R') { ... }` branch of
handleOTDirectCommand() is cut from src/OTGW-firmware/OTDirect.ino and compiled with doubles for the state it reads.
Its replies are then fed, line by line, into the REAL pyotgw CommandProcessor.issue_cmd() and
OpenThermGateway.get_reports() from other-projects/pyotgw-master (version 2.2.3), through a fake transport.

Old-vs-fix: OTGW_OTD_INO points at another copy of OTDirect.ino, e.g.
  git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
  OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_pr_reports_pyotgw.py
Run: python tests/test_otd_pr_reports_pyotgw.py
"""
import asyncio
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
PYOTGW = REPO / "other-projects" / "pyotgw-master"
LETTERS = "ABCDGILMOPQRSTVW"     # pyotgw vars.OTGW_REPORTS

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#define PSTR(s) (s)
#define snprintf_P snprintf
#define OTDDebugTf(...) do {} while (0)
static char* dtostrf(double v, int w, int p, char* out) { sprintf(out, "%*.*f", w, p, v); return out; }
enum { OT_OVERRIDE_NONE = 0, OT_OVERRIDE_TEMPORARY = 1, OT_OVERRIDE_CONSTANT = 2 };
struct { int mode; uint16_t f88Value; bool bAccepted; } otRemoteOverride = { OT_OVERRIDE_NONE, 0, false };
static bool otRemoteOverrideClearPending = false;
struct { bool tx = false, rx = false; bool txRaised() const { return tx; } bool rxInverted() const { return rx; } } otSlave;
static char otForceThermostat = 'D';
#define OT_OVERRIDE_COUNT 4
struct { bool active; uint8_t msgId; uint16_t overrideValue; } otOverrides[OT_OVERRIDE_COUNT] = {};
static float otSetbackTemp = 16.0f;
static uint8_t otDHWOverride = 0xFF;
static char otGpioFunctions[3] = "00";
static char otLedFunctions[7] = "FXOMPC";
static char otTempSensor = 'O';
static uint8_t otIgnoreTransitions = 1;
static uint8_t otOverrideHB = 0;
static uint32_t otMinIntervalMs = 100;
static uint8_t otVoltageRef = 3;
static char platformGetResetReasonChar() { return 'P'; }
static int g_mode = 'G';
#define IS_BYPASS_MODE() (g_mode == 'P')
#define IS_MONITOR_MODE() (g_mode == 'M')
#define IS_MASTER_MODE() (g_mode == 'S')
#define IS_LOOPBACK_MODE() (g_mode == 'L')
static void otDirectBridgeProcessPRResponse(const char* line) { printf("%s\n", line); }
"""

MAIN = r"""
int main(int argc, char** argv) {
  // argv[1]: letters to query; argv[2], argv[3]: IT and OH values
  if (argc > 3) { otIgnoreTransitions = (uint8_t)atoi(argv[2]); otOverrideHB = (uint8_t)atoi(argv[3]); }
  // TASK-1848: further state as key=value arguments; ftmap= and stamp= call the real helpers directly.
  for (int a = 4; a < argc; a++) {
    const char* kv = argv[a];
    if (!strncmp(kv, "tx=", 3))  otSlave.tx = atoi(kv + 3);
    if (!strncmp(kv, "rx=", 3))  otSlave.rx = atoi(kv + 3);
    if (!strncmp(kv, "ovr=", 4)) otRemoteOverride.mode = atoi(kv + 4);
    if (!strncmp(kv, "f88=", 4)) otRemoteOverride.f88Value = (uint16_t)strtoul(kv + 4, nullptr, 0);
    if (!strncmp(kv, "acc=", 4)) otRemoteOverride.bAccepted = atoi(kv + 4);
    if (!strncmp(kv, "clr=", 4)) otRemoteOverrideClearPending = atoi(kv + 4);
    if (!strncmp(kv, "cs=", 3))  { otOverrides[0].active = true; otOverrides[0].msgId = 1;
                                   otOverrides[0].overrideValue = (uint16_t)strtoul(kv + 3, nullptr, 0); }
    if (!strncmp(kv, "ft=", 3))  otForceThermostat = kv[3];
#ifdef HAVE_PR_HELPERS
    if (!strncmp(kv, "ftmap=", 6)) printf("FT %c\n", otdThermostatModelChar(kv[6]));
    if (!strncmp(kv, "stamp=", 6)) {
      char d[16] = {0}; const char* bar = strchr(kv + 6, '|');
      memcpy(d, kv + 6, (size_t)(bar - (kv + 6))); char out[20];
      otdPicBuildStamp(d, bar + 1, out, sizeof(out));
      printf("STAMP %s\n", out);
    }
#endif
  }
  for (const char* p = argv[1]; *p; p++) {
    char value[2] = { *p, 0 };
    printf("Q %c\n", *p);
    prQuery(value);
  }
  return 0;
}
"""


def extract_pr_branch(src: str) -> str:
    """The `else if (cmd0 == 'P' && cmd1 == 'R') { ... }` branch, braces matched."""
    m = re.search(r"else if \(cmd0 == 'P' && cmd1 == 'R'\) \{", src)
    if not m:
        raise AssertionError("PR= branch not found in " + str(OTD_INO))
    i, depth = m.end() - 1, 0
    for j in range(i, len(src)):
        c = src[j]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    raise AssertionError("unbalanced PR= branch")


def build_source() -> str:
    src = OTD_INO.read_text(encoding="utf-8")
    branch = extract_pr_branch(src)
    # the real version macro the PR=A reply uses (absent in the code before TASK-1797)
    defs = "".join(m.group(0) + "\n" for m in re.finditer(r"^#define OTD_PIC_COMPAT_VERSION .*$", src, re.M))
    # TASK-1848: the real PR= helpers (absent in the code before TASK-1848)
    h = re.search(r"// --- otdPrHelpers begin\n(.*?)// --- otdPrHelpers end", src, re.S)
    if h:
        defs += "#define HAVE_PR_HELPERS 1\n" + h.group(1)
    fn = ("static void prQuery(const char* value) {\n  const char cmd0 = 'P', cmd1 = 'R';\n  char rspBuf[16];\n"
          "  if (false) {}\n  " + branch + "\n}\n")
    return STUBS + defs + fn + MAIN


_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    _tmp = Path(tempfile.mkdtemp(prefix="otd_pr_"))
    cpp = _tmp / "otd_pr.cpp"
    cpp.write_text(build_source(), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("otd_pr.exe" if os.name == "nt" else "otd_pr"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run_exe(letters, it=1, oh=0, **kv) -> str:
    args = [str(_exe), letters, str(it), str(oh)] + [f"{k}={v}" for k, v in kv.items()]
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout


def replies(it=1, oh=0, letters=LETTERS, **kv) -> dict:
    out = run_exe(letters, it, oh, **kv)
    res, cur = {}, None
    for line in out.splitlines():
        if line.startswith("Q "):
            cur = line[2]
        elif cur:
            res[cur] = line
    return res


def run_pyotgw_get_reports(reply_map: dict):
    """Real pyotgw get_reports() with a transport that answers each PR=X with the OT-Direct line."""
    sys.path.insert(0, str(PYOTGW))
    try:
        from pyotgw.commandprocessor import CommandProcessor
        from pyotgw.pyotgw import OpenThermGateway
    except ImportError as exc:      # serial_asyncio_fast missing
        raise unittest.SkipTest(f"pyotgw not importable: {exc}")
    asked = []

    async def go():
        gw = OpenThermGateway()

        class Transport:
            def write(self, data):
                cmd = data.decode("ascii").strip()
                asked.append(cmd)
                letter = cmd[3:] if cmd.startswith("PR=") else ""
                line = reply_map.get(letter)
                if line is not None:
                    asyncio.get_running_loop().call_soon(proto.command_processor.submit_response, line)

        class Proto:
            connected = True
            transport = Transport()

        proto = Proto()
        proto.command_processor = CommandProcessor(proto, gw.status)
        gw._protocol = proto

        class Conn:
            connected = True
        gw.connection = Conn()
        try:
            return await gw.get_reports()
        finally:
            await gw.status.cleanup()

    return asyncio.run(go()), asked


class OTDirectPRReports(unittest.TestCase):
    def test_A1_banner_is_pic_version_then_the_board(self):
        a = replies()["A"]
        self.assertRegex(a, r"^PR: A=OpenTherm Gateway \d+(\.\d+)+ OTGW32$", a)
        about = a[len("PR: "):][2:]
        self.assertTrue(about[18].isdigit(), f"pyotgw int(ver[18]) on {about!r}")

    def test_A2_otmonitor_reads_a_version_that_enables_the_6x_features(self):
        a = replies()["A"]
        words = a[a.index("OpenTherm Gateway "):].split()
        self.assertGreaterEqual(len(words), 3, a)
        ver = tuple(int(x) for x in words[2].split("."))
        self.assertGreaterEqual(ver, (6, 2), f"gui.tcl:1080 gates on 6.2; got {words[2]}")

    def test_T1_tweaks_are_two_digits_without_separator(self):
        for it, oh in ((1, 0), (0, 1), (1, 1), (0, 0)):
            t = replies(it, oh)["T"]
            self.assertEqual(t, f"PR: T={it}{oh}")
            self.assertRegex(t, r"^PR: T=[01][01]$")   # otmonitor.tcl:631

    def test_P1_pyotgw_get_reports_runs_on_the_ot_direct_replies(self):
        rm = replies(1, 0)
        status, asked = run_pyotgw_get_reports(rm)
        otgw = status["gateway"]      # pyotgw vars.OTGW
        self.assertEqual(otgw["otgw_about"], rm["A"][len("PR: A="):])
        self.assertEqual(otgw["otgw_ignore_transitions"], 1)
        self.assertEqual(otgw["otgw_ovrd_high_byte"], 0)
        self.assertIn("PR=D", asked, "pyotgw skips PR=D when the banner version is below 5")
        self.assertEqual(otgw.get("otgw_temp_sensor"), "O")


    def test_X1_pr_x_reports_the_compat_version_from_the_banner_macro(self):
        # TASK-1860: an OT-Direct-only report (the PIC 6.8 answers BV for PR=X, gateway.asm:2163-2189).
        rm = replies(1, 0, LETTERS + "X")
        self.assertIn("X", rm, "PR=X gives no reply")
        self.assertRegex(rm["X"], r"^PR: X=\d+\.\d+$", rm["X"])
        banner_ver = rm["A"][len("PR: A=OpenTherm Gateway "):].split()[0]
        self.assertEqual(rm["X"][len("PR: X="):], banner_ver, "PR=X and the PR=A banner must carry the same version")


def pic_print_float(f88: int) -> str:
    """PrintFloat of PIC 6.8 (gateway.asm:2049-2066) for a positive f8.8: a low byte of 0xFF rounds up to the
    next whole degree, else the hundredths are (low * 100) >> 8, rounded up when bit 7 of the product's low byte
    is set; printed as "<int>.<2 digits>"."""
    hi, lo = (f88 >> 8) & 0xFF, f88 & 0xFF
    if lo == 0xFF:
        hi, lo = hi + 1, 0
    prod = lo * 100
    frac = (prod >> 8) + (1 if prod & 0x80 else 0)
    return f"{hi}.{frac:02d}"


class PicReportTexts(unittest.TestCase):
    """TASK-1848: PR=P, R, O, C, B and L give the PIC 6.8 text, byte for byte (gateway.asm 6.8 at 770aa45)."""

    def test_P_power_level_follows_the_two_line_levels(self):
        # PrintPowerlevel gateway.asm:2262-2268, strings :5499-5502 ("Low" runs on into " power")
        for tx, rx, text in ((0, 0, "Low power"), (0, 1, "Low power"), (1, 0, "Medium power"), (1, 1, "High power")):
            self.assertEqual(replies(letters="P", tx=tx, rx=rx)["P"], f"PR: P={text}", (tx, rx))

    def test_R_default_is_D_and_follows_FT(self):
        self.assertEqual(replies(letters="R")["R"], "PR: R=D")          # PrintRemeha :2270-2280, no flags = D
        for ft in "CIS":
            self.assertEqual(replies(letters="R", ft=ft)["R"], f"PR: R={ft}")

    def test_R_ft_argument_maps_like_SetTStatModel(self):
        # SetTStatModel :4705-4727: C, I, S set by hand, anything else is auto-detect (D); R needs a detection
        out = run_exe("", 1, 0, **{"ftmap": "C"}) + run_exe("", 1, 0, ftmap="I") + run_exe("", 1, 0, ftmap="S") \
            + run_exe("", 1, 0, ftmap="D") + run_exe("", 1, 0, ftmap="A") + run_exe("", 1, 0, ftmap="R")
        self.assertEqual(out.split(), ["FT", "C", "FT", "I", "FT", "S", "FT", "D", "FT", "D", "FT", "D"])

    def test_O_none_clearing_pending_and_accepted(self):
        # PrintOverride :2201-2226
        self.assertEqual(replies(letters="O")["O"], "PR: O=N")
        self.assertEqual(replies(letters="O", clr=1)["O"], "PR: O=n")
        for mode, up in ((1, "T"), (2, "C")):
            for f88 in (0x1480, 0x14FF, 0x1402, 0x1401, 0x1300, 0x0F33):
                want = pic_print_float(f88)
                self.assertEqual(replies(letters="O", ovr=mode, f88=f88, acc=0)["O"], f"PR: O={up.lower()}{want}",
                                 (mode, hex(f88)))
                self.assertEqual(replies(letters="O", ovr=mode, f88=f88, acc=1)["O"], f"PR: O={up}{want}",
                                 (mode, hex(f88)))

    def test_O_a_cs_override_alone_is_not_a_room_setpoint_override(self):
        # CS= never touches the PIC's override state (SetSetPoint :4095 serves TT/TC only)
        self.assertEqual(replies(letters="O", cs=0x2D00)["O"], "PR: O=N")

    def test_C_clock_in_the_pic_format(self):
        self.assertRegex(replies(letters="C")["C"], r"^PR: C=\d+ MHz$")   # SpeedString :5506-5507 "4 MHz"

    def test_B_build_stamp_in_the_pic_format(self):
        self.assertRegex(replies(letters="B")["B"], r"^PR: B=\d\d:\d\d \d\d-\d\d-\d{4}$")   # build.tcl:83
        for date, time, want in (("Nov 14 2026", "14:05:33", "14:05 14-11-2026"),
                                 ("Jan  4 2027", "09:00:01", "09:00 04-01-2027"),
                                 ("Dec 31 2026", "23:59:59", "23:59 31-12-2026")):
            out = run_exe("", 1, 0, stamp=f"{date}|{time}")
            self.assertIn(f"STAMP {want}", out, (date, time))

    def test_L_default_is_the_pic_default(self):
        src = OTD_INO.read_text(encoding="utf-8")
        self.assertRegex(src, r'static char\s+otLedFunctions\[7\]\s*=\s*"FXOMPC";')   # :5462-5468


class DeviceInfoCompatField(unittest.TestCase):
    """TASK-1860 AC2: /api/v2/device/info carries the same version as "otdpiccompat", only on OT-Direct.

    Source-level check on the real emitter (sendDeviceInfoV2() is a chunked lambda over the whole device
    state, not sliceable into a host binary): the field sits in the `if (snap->otDirectEnabled)` block, and
    otdPicCompatVersion() returns the same macro PR=A and PR=X print. The rendered JSON is a bench AC.
    """

    def test_field_is_emitted_only_in_the_ot_direct_block(self):
        rest = Path(os.environ.get("OTGW_REST_INO", REPO / "src" / "OTGW-firmware" / "restAPI.ino")).read_text(encoding="utf-8")
        fn = rest[rest.index("void sendDeviceInfoV2()"):]
        block = fn[fn.index("if (snap->otDirectEnabled) {"):fn.index("#endif", fn.index("if (snap->otDirectEnabled) {"))]
        self.assertIn('je.field(F("otdpiccompat"), otdPicCompatVersion());', block)
        self.assertEqual(fn.count('F("otdpiccompat")'), 1, "emitted once, in the OT-Direct block only")

    def test_helper_returns_the_banner_macro(self):
        src = OTD_INO.read_text(encoding="utf-8")
        self.assertRegex(src, r'const char\* otdPicCompatVersion\(\) \{ return OTD_PIC_COMPAT_VERSION; \}')


if __name__ == "__main__":
    unittest.main(verbosity=2)
