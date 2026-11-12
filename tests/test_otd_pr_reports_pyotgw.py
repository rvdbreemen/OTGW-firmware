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
struct { int mode; uint16_t f88Value; } otRemoteOverride = { OT_OVERRIDE_NONE, 0 };
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


def replies(it=1, oh=0) -> dict:
    out = subprocess.run([str(_exe), LETTERS, str(it), str(oh)], capture_output=True, text=True, check=True).stdout
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
