#!/usr/bin/env python3
"""TASK-1801: OT-Direct never blacklists the Data-IDs the PIC treats as mandatory, and a success enables an entry
the 3-strike rule disabled.

PIC ground truth (OTGW-other-projects/otgw-6.8/gateway.asm, repo 770aa45): MandatoryID :3409-3416 unmarks the ID
in unknownmap on every request ("so even (accidental) manual action cannot blacklist the DataID"); called for 0
(readstatus :2720), 1 (MessageID1 :2806), 14 (MessageID14 :2967) and 3, 17, 25 (MandatoryWord :2544, :2558, :2566,
:3651).

The code under test is REAL, cut from src/OTGW-firmware/OTDirect.ino: the otUnknownCounters helpers, isUnknownId(),
the "// --- otdUnknownStrike" block and otdIsAltSlot(). On a revision without the block the 3-strike code is the
inline block of handleMasterResponse() (from "// 3-strike auto-blacklist" to the TASK-1845 line); it is cut out as
is and wrapped in the same signature, so the old run also tests real code. The schedule is a stand-in table.

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_mandatory_ids.py
Run: python tests/test_otd_mandatory_ids.py
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
#define PSTR(s) (s)
#define OTDDebugTf(...) do {} while (0)
enum class OpenThermMessageType { READ_ACK = 4, UNKNOWN_DATA_ID = 7 };
struct OpenTherm { static OpenThermMessageType getMessageType(unsigned long r) { return (OpenThermMessageType)((r >> 28) & 7); } };
struct OTScheduleEntry { uint8_t msgId; bool disabled; };
static OTScheduleEntry otSchedule[] = { {1, false}, {3, false}, {14, false}, {17, false}, {25, false}, {5, false},
                                        {27, false}, {56, false} };
static constexpr uint8_t OT_SCHEDULE_SIZE = sizeof(otSchedule) / sizeof(otSchedule[0]);
static constexpr uint8_t OT_UNKNOWN_ID_MAX = 16;
static uint8_t otUnknownIds[OT_UNKNOWN_ID_MAX];
static uint8_t otUnknownIdCount = 0;
static bool otdRemoteOverrideHoldsSlot(uint8_t) { return false; }
static bool otdResponseOverrideActive(uint8_t) { return false; }
"""

MAIN = r"""
static bool disabledOf(uint8_t id) { for (auto& e : otSchedule) if (e.msgId == id) return e.disabled; return false; }
int main(int argc, char** argv) {
  // argv: "U<id>" Unknown-DataID answer, "S<id>" success answer, "M<id>" UI= mark, "D<id>" DA= disable, "Q<id>" query
  for (int a = 1; a < argc; a++) {
    const char k = argv[a][0]; const uint8_t id = (uint8_t)atoi(argv[a] + 1);
    if (k == 'U') otdNoteBoilerAnswer(id, true);
    else if (k == 'S') otdNoteBoilerAnswer(id, false);
    else if (k == 'M') otUnknownIds[otUnknownIdCount++] = id;
    else if (k == 'D') { for (auto& e : otSchedule) if (e.msgId == id) e.disabled = true; }
    else printf("%u count=%u disabled=%d alt=%d\n", id, getUnknownCount(id), disabledOf(id) ? 1 : 0, otdIsAltSlot(id) ? 1 : 0);
  }
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    counters = SRC[SRC.index("static uint8_t otUnknownCounters[32]"):]
    counters = counters[:counters.index("\n}\n", counters.index("static void clearUnknownCount(")) + 3]
    is_unknown = braced(SRC, r"^static bool isUnknownId\(uint8_t msgId\)\s*\{")
    blk = re.search(r"// --- otdUnknownStrike begin\n(.*?)// --- otdUnknownStrike end", SRC, re.S)
    if blk:
        strike = blk.group(1)
    else:   # before TASK-1801: the inline block of handleMasterResponse(), wrapped as is
        a = SRC.index("    // 3-strike auto-blacklist")
        b = SRC.index("    // TASK-1845", a)
        strike = ("static void otdNoteBoilerAnswer(uint8_t id, bool unknownDataId) {\n"
                  "  unsigned long response = ((unsigned long)(unknownDataId ? 7 : 4) << 28) | ((unsigned long)id << 16);\n"
                  + SRC[a:b] + "}\n")
    alt = braced(SRC, r"^static bool otdIsAltSlot\(uint8_t msgId\)\s*\{")
    src = PRE + counters + "\n" + is_unknown + "\n" + strike + "\n" + alt + "\n" + MAIN
    _tmp = Path(tempfile.mkdtemp(prefix="otd_mand_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text("#include <cstdlib>\n" + src, encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(*args):
    out = subprocess.run([str(_exe)] + list(args), capture_output=True, text=True, check=True).stdout.splitlines()
    return {line.split()[0]: dict(f.split("=") for f in line.split()[1:]) for line in out}


class MandatoryIds(unittest.TestCase):
    def test_mandatory_ids_are_never_disabled_or_alternative_slots(self):
        for mid in (1, 3, 14, 17, 25):
            r = run(f"U{mid}", f"U{mid}", f"U{mid}", f"U{mid}", f"Q{mid}")[str(mid)]
            self.assertEqual((r["disabled"], r["alt"]), ("0", "0"), mid)

    def test_a_ui_mark_does_not_make_a_mandatory_id_an_alternative(self):
        r = run("M14", "Q14", "M5", "Q5")
        self.assertEqual(r["14"]["alt"], "0")
        self.assertEqual(r["5"]["alt"], "1")    # a normal ID with a UI= mark stays an alternative

    def test_other_ids_keep_the_three_strike_rule(self):
        r = run("U5", "U5", "Q5")["5"]
        self.assertEqual((r["count"], r["disabled"], r["alt"]), ("2", "0", "0"))
        r = run("U5", "U5", "U5", "Q5")["5"]
        self.assertEqual((r["count"], r["disabled"], r["alt"]), ("3", "1", "1"))

    def test_a_success_enables_a_struck_entry_again(self):
        r = run("U5", "U5", "U5", "S5", "Q5")["5"]
        self.assertEqual((r["count"], r["disabled"], r["alt"]), ("0", "0", "0"))

    def test_a_data_invalid_answer_resets_the_strikes_like_the_pic(self):
        # TASK-1804 E6: the PIC counts only Unknown-DataID answers and resets on any other, Data-Invalid included
        # (HandleResponse / MsgSupported, gateway.asm 6.8 :2440-2455). Source check: handleMasterResponse() feeds a
        # Data-Invalid answer to the rule as a non-Unknown answer; behaviour of that rule: Unknown, Unknown, a
        # non-Unknown answer, Unknown leaves the ID enabled with one strike.
        h = braced(SRC, r"^static void handleMasterResponse\(\) \{") or ""
        i = h.find("} else if (boilerDataInvalid) {")
        self.assertGreater(i, 0, "Data-Invalid branch of handleMasterResponse() not found")
        branch = h[i:h.find("} else {", i)]
        self.assertRegex(branch, r"otdNoteBoilerAnswer\(\(response >> 16\) & 0xFF, false\)")
        r = run("U5", "U5", "S5", "U5", "Q5")["5"]
        self.assertEqual((r["count"], r["disabled"]), ("1", "0"), "not blacklisted, as on the PIC")

    def test_a_success_does_not_undo_da_or_ui(self):
        r = run("D56", "S56", "Q56", "M27", "D27", "S27", "Q27")
        self.assertEqual(r["56"]["disabled"], "1")   # DA= stays
        self.assertEqual(r["27"]["disabled"], "1")   # UI= stays


if __name__ == "__main__":
    unittest.main(verbosity=2)
