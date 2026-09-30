#!/usr/bin/env python3
"""TASK-1123 host harness: the DHW water total (ADR-176) through the real code.

    python test/host/test_dhw_water_meter.py                  # FIX, mutants, discovery
    python test/host/test_dhw_water_meter.py --old-rev HEAD   # plus the OLD build

Two programs are compiled from code sliced by anchor out of the sources, never copied:

1. test_dhw_water_meter.cpp: dhwWaterMeter.ino (the accumulator, the write-rate rule,
   save / flush / load) and, from OTGW-Core.ino, print_f88() and
   updatePSSummaryFloatState(), the two MsgID 19 state write sites ADR-176 names, with
   the validity gates print_f88() calls and parseStrictFloat(), the PS=1 field parser.
2. test_dhw_water_discovery.cpp: the discovery tables and indexes from
   MQTTHaDiscovery.cpp and, from MQTTstuff.ino, the paths that queue a discovery
   config (queueNonOTDiscoveryIds() via the boot path, markAllMQTTConfigPending()),
   both device maps for faux id 241, the state publish (sendDHWWaterTotal(),
   publishDHWWaterMeter()), and the reset (ADR-176 Q6): dhwWaterMeter.ino's queue and
   loop-side consumer, the reset_water_total branch of handleOtgw() (restAPI.ino) and
   the otgw/ branch of handleMQTTcallback() (MQTTstuff.ino), each run in a wrapper.

The call sites that wire the meter into the firmware are checked statically, in the
function bodies (signature line through the closing brace at column 0) of setup(),
doTaskEvery60s(), loop() (OTGW-firmware.ino), doRestart() (helperStuff.ino),
handleMQTTcallback() (MQTTstuff.ino) and handleOtgw() (restAPI.ino): each call or
branch once, and in the order that matters. In loop() that order puts the reset after
doBackgroundTasks() and before drainOTFrameQueue() and the deferred reboot, so a reset
that waited for a flash to end runs before the reboot the same flash may have asked for. Removing each must flip its check. Two
more static checks, each with its own flip: the REST route table sends
/api/v2/otgw/* to handleOtgw(), and every clearMQTTConfigDone() call is followed in
its function by a call that queues the non-OT ids again (so a republish re-announces
241 without help from the state publisher).

dhwWaterMeter.ino does not exist before TASK-1123, so OLD cannot show an accumulator
of its own. With --old-rev, program 1 is built a second time with OTGW-Core.ino (and
its headers) at that revision: the same driver and the same meter module, only the
write sites differ. OLD must fail exactly the cases that need the write sites to feed
the meter and pass all others. The wiring is reported for OLD as well. The round-1
announce gating (241 queued only after a MsgID 19 sample) is replayed as mutant M15.
Two round-2 defects the round-2 review found are replayed the same way: the reset
consumer that ran while the file could not be written (MR13, case X6), and the flow
check that let NaN through (M25, case W6, through the real PS=1 field parser).

Mutants of the FIX slices must each fail their named case. Every substitution must
match exactly once and change the text. A mutant marked crash_ok may also end the
process instead (the /GS stack check on a buffer overrun): its case must then be
missing or FAIL and the exit code non-zero.

Exit code: 0 PASS, 1 FAIL, 2 harness error. A slice or compile failure is a harness
error, never a reproduction.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

from test_ot_reserved_range import ANCHORS as RESERVED_ANCHORS
from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars, slice_by_anchor
from test_pic_banner_dispatch import slice_range

FW = "src/OTGW-firmware/"
CORE_H, CORE, FW_H, HW_H = FW + "OTGW-Core.h", FW + "OTGW-Core.ino", FW + "OTGW-firmware.h", FW + "Hardwaretypes.h"
METER, MQTT_H, MQTT_INO, DISC = FW + "dhwWaterMeter.ino", FW + "MQTTstuff.h", FW + "MQTTstuff.ino", FW + "MQTTHaDiscovery.cpp"
FW_INO, HELPER, REST_INO = FW + "OTGW-firmware.ino", FW + "helperStuff.ino", FW + "restAPI.ino"

METER_CASES = ["W1", "W2", "W3", "W4", "W5", "W6", "R1", "U1", "U2", "U3", "U4", "U5", "U6", "U7",
               "P1", "P2", "P3", "P4", "P5", "P6", "P7"]
OLD_DEFECT = ["W1", "W2", "W4", "W5", "W6", "R1"]    # need the write sites to feed the meter
DISC_CASES = ["D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10", "D11",
              "X1", "X2", "X3", "X4", "X5", "X6", "X7", "Q1", "Q2", "Q3", "Q4", "Q5"]

HELPER_241 = "setMQTTConfigPending(OTGWdhwmeterid);     // 241 TASK-1123 DHW water total (ADR-176; its state waits for MsgID 19)"
GATED_241 = "if (dhwWaterMeterHasData()) setMQTTConfigPending(OTGWdhwmeterid);     // 241"
WALK = "      setMQTTConfigPending(static_cast<uint8_t>(i));"
STATE_GATE = "  if (!dhwWaterMeterHasData()) return;\n  sendDHWWaterTotal();"
SUBCMD_ASSERT = ('          static_assert(sizeof("reset_water_total") < sizeof(otgwSubCmd),\n'
                 '                        "otgwSubCmd must hold reset_water_total plus one more character, '
                 'so a longer token cannot truncate into it");\n')
FLOW_CHECK = "!(flowLitresPerMin > 0.0f) || flowLitresPerMin > DHW_METER_MAX_FLOW_LPM"
RESET_GATE = "  if (!LittleFSmounted || isFlashing()) return;   // the file cannot be written: the reset waits"
RESET_ZERO = "    dhwWaterTotalL = 0.0;\n    writeDHWWaterMeterFile(millis());\n"
SKIP_ZERO = "  if (dhwWaterTotalL != 0.0 || dhwMeterSavedL != 0.0) {   // RAM or file not at 0 yet"

# (id, what, generated file, [(old text, new text), ...], case that must fail, cases that must still pass[, crash_ok])
METER_MUTANTS = [
    ("M1", "gap cap removed", "gen_meter.inc",
     [("if (dtMs > DHW_METER_MAX_GAP_MS) return;", "")], "U2", []),
    ("M2", "boiler-only source filter removed from print_f88()", "gen_core.inc",
     [("OTdata.id == 19 && OTdata.rsptype == OTGW_BOILER", "OTdata.id == 19")], "W3", ["W4"]),
    ("M3", "OT-Direct summary filter removed from updatePSSummaryFloatState()", "gen_core.inc",
     [("if (!isOTDirectEnabled()) updateDHWWaterMeter(fval, millis());", "updateDHWWaterMeter(fval, millis());")],
     "W5", ["W2"]),
    ("M7", "restart flush writes only a missing file (unsaved branch removed)", "gen_meter.inc",
     [("if (unsaved || fileGone) writeDHWWaterMeterFile(millis());", "if (fileGone) writeDHWWaterMeterFile(millis());")],
     "P2", ["P3"]),
    ("M8", "restart flush ignores a missing file (fileGone branch removed)", "gen_meter.inc",
     [("if (unsaved || fileGone) writeDHWWaterMeterFile(millis());", "if (unsaved) writeDHWWaterMeterFile(millis());")],
     "P3", ["P2"]),
    ("M9", "loader upper bound removed", "gen_meter.inc",
     [(" || litres > DHW_METER_MAX_LITRES) return;", ") return;")], "P5", ["P4"]),
    ("M10", "read-back check removed from the file write", "gen_meter.inc",
     [("if (strcmp(back, buf) != 0) return false;", "")], "P7", ["P1"]),
    # The persistence and accumulator rules the round-2 review mutated in its scratchpad.
    ("M18", "isFlashing() guard removed from the file write", "gen_meter.inc",
     [("  if (!LittleFSmounted || isFlashing()) return false;\n", "  if (!LittleFSmounted) return false;\n")],
     "P6", ["P1"]),
    ("M19", "save threshold 1 L instead of 10 L", "gen_meter.inc",
     [("DHW_METER_SAVE_DELTA_L     = 10.0;", "DHW_METER_SAVE_DELTA_L     = 1.0;")], "P1", ["P4"]),
    ("M20", "15-minute save rule removed", "gen_meter.inc",
     [("  return (nowMs - dhwMeterSavedMs) >= DHW_METER_SAVE_INTERVAL_MS; // the remainder of a draw\n",
       "  return false;\n")], "P1", ["P2"]),
    ("M21", "idle ticks write (the zero-unsaved return removed)", "gen_meter.inc",
     [("  if (unsaved == 0.0) return false;                               // the file holds the total: no flash write\n",
       "")], "P1", ["P2"]),
    ("M22", "the loader does not set the saved mark", "gen_meter.inc",
     [("  dhwMeterSavedL = litres;\n", "")], "P4", ["P5"]),
    ("M23", "the clock does not move across a gap", "gen_meter.inc",
     [("  dhwMeterLastMs = nowMs;\n\n  if (dtMs > DHW_METER_MAX_GAP_MS) return;        // gap: never counted as water\n",
       "\n  if (dtMs > DHW_METER_MAX_GAP_MS) return;        // gap: never counted as water\n  dhwMeterLastMs = nowMs;\n")],
     "U2", ["U1"]),
    # Round-2 fixup, review issue 2: a flow f8.8 cannot carry adds nothing.
    ("M24", "zero and negative flow counted (lower bound removed)", "gen_meter.inc",
     [(FLOW_CHECK, "flowLitresPerMin > DHW_METER_MAX_FLOW_LPM")], "U6", ["U1"]),
    ("M25", "NaN counted: the round-2 check (flow <= 0) replayed", "gen_meter.inc",
     [("!(flowLitresPerMin > 0.0f)", "flowLitresPerMin <= 0.0f")], "W6", ["U6"]),
    ("M26", "no upper flow bound", "gen_meter.inc",
     [(FLOW_CHECK, "!(flowLitresPerMin > 0.0f)")], "U7", ["U6"]),
]
DISC_MUTANTS = [
    # Item 1: 241 is queued at boot and on every full republish, unconditionally.
    ("M4", "241 line removed from queueNonOTDiscoveryIds()", "gen_disc_mqtt.inc",
     [("  " + HELPER_241 + "\n", "")], "D4", ["D5"]),
    ("M5", "round-1 data gate restored in queueNonOTDiscoveryIds()", "gen_disc_mqtt.inc",
     [(HELPER_241, GATED_241)], "D4", ["D5", "D7"]),
    ("M15", "round-1 announce gating restored (helper data gate plus the markAll walk skip)", "gen_disc_mqtt.inc",
     [(HELPER_241, GATED_241), (WALK, "      if (i == OTGWdhwmeterid) continue;\n" + WALK)], "D5", ["D7"]),
    ("M17", "AC#2 state gate removed from publishDHWWaterMeter()", "gen_disc_mqtt.inc",
     [(STATE_GATE, "  sendDHWWaterTotal();")], "D6", ["D7"]),
    ("M6", "MQTT_HA_SENSOR_COUNT left at 389", "gen_disc_table.inc",
     [("const uint16_t MQTT_HA_SENSOR_COUNT = 390;", "const uint16_t MQTT_HA_SENSOR_COUNT = 389;")], "D1", []),
    ("M11", "publisher formats with dtostrf into msg[24] (the pre-fixup code)", "gen_disc_mqtt.inc",
     [('snprintf_P(msg, sizeof(msg), PSTR("%.1f"), dhwWaterTotalL);', "dtostrf(dhwWaterTotalL, 0, 1, msg);")],
     "D11", ["D7", "D10"], True),
    ("M12", "case 241 removed from deviceForOTId()", "gen_disc_mqtt.inc",
     [('case 241: return HaDevice::Sensors;     // TASK-1123 DHW water total: fixed "sensors_" uniq_id prefix', "")],
     "D3", ["D1"]),
    ("M13", "case 241 removed from topoDeviceForPseudoId()", "gen_disc_helpers.inc",
     [("case 241: return HaDevice::Sensors;     // TASK-1123 DHW water total", "")], "D3", ["D1"]),
    ("M14", "loader upper bound removed, seen from the publish side", "gen_meter.inc",
     [(" || litres > DHW_METER_MAX_LITRES) return;", ") return;")], "D10", ["D11"]),
    # Item 2: the reset.
    ("MR1", "the reset writes no file", "gen_meter.inc",
     [(RESET_ZERO, "    dhwWaterTotalL = 0.0;\n")], "X1", []),
    ("MR2", "the reset leaves the RAM total", "gen_meter.inc",
     [(RESET_ZERO, "    writeDHWWaterMeterFile(millis());\n")], "X1", []),
    ("MR3", "the reset publishes through the data-gated 60 s publisher", "gen_meter.inc",
     [("  sendDHWWaterTotal();\n  DebugTln(", "  publishDHWWaterMeter();\n  DebugTln(")], "X3", ["X1"]),
    ("MR4", "the reset flag is never cleared", "gen_meter.inc",
     [("  dhwMeterResetPending = false;   // clear first; a request raised after this line is served next pass\n", "")],
     "X1", []),
    ("MR5", "the save rule ignores a total below the file (round-1 rule)", "gen_meter.inc",
     [("  if (unsaved == 0.0) return false;                               // the file holds the total: no flash write\n"
       "  if (unsaved < 0.0) return true;                                 // a reset whose file write failed\n",
       "  if (unsaved <= 0.0) return false;                               // nothing new: no flash write\n")],
     "X4", ["X1", "X2"]),
    ("MR6", "the restart flush writes only a larger total (round-1 rule)", "gen_meter.inc",
     [("(dhwWaterTotalL != dhwMeterSavedL)", "(dhwWaterTotalL > dhwMeterSavedL)")], "X4", ["X1"]),
    ("MR7", "queueDHWWaterMeterReset() raises no flag", "gen_meter.inc",
     [("  dhwMeterResetPending = true;\n", "")], "X1", ["D7"]),
    ("MR8", "the REST branch does not queue the reset", "gen_rest_reset_branch.inc",
     [("    queueDHWWaterMeterReset();\n", "")], "Q1", ["Q2"]),
    ("MR9", "the REST branch accepts any method", "gen_rest_reset_branch.inc",
     [('    if (method != HTTP_POST) { sendApiMethodNotAllowed(F("POST")); return; }\n', "")], "Q2", ["Q1"]),
    ("MR10", "the MQTT branch does not queue the reset", "gen_mqtt_otgw_branch.inc",
     [("              queueDHWWaterMeterReset();\n", "")], "Q3", ["Q4"]),
    ("MR23", "the MQTT branch honours a retained reset", "gen_mqtt_otgw_branch.inc",
     [("            if (retained) {\n", "            if (false) {\n")], "Q5", ["Q3", "Q4"]),
    ("MR11", "the MQTT branch resets on any sub-command", "gen_mqtt_otgw_branch.inc",
     [('strcasecmp_P(otgwSubCmd, PSTR("reset_water_total")) == 0', "true")], "Q4", ["Q3"]),
    ("MR12", "MQTT sub-command buffer shrunk to 18 bytes with its static_assert removed", "gen_mqtt_otgw_branch.inc",
     [(SUBCMD_ASSERT, ""), ("char otgwSubCmd[24];", "char otgwSubCmd[18];")], "Q4", ["Q3"]),
    # Round-2 fixup, review issue 1 (blocking): the reset waits while the file cannot be
    # written. MR13 is the round-2 consumer itself, so it is the OLD side of that fix.
    ("MR13", "the round-2 consumer replayed (no file-writable gate)", "gen_meter.inc",
     [(RESET_GATE + "\n", "")], "X6", ["X1"]),
    ("MR14", "the reset gate without its LittleFSmounted test", "gen_meter.inc",
     [(RESET_GATE, RESET_GATE.replace("!LittleFSmounted || ", ""))], "X6", ["X1"]),
    ("MR15", "the reset gate without its isFlashing() test", "gen_meter.inc",
     [(RESET_GATE, RESET_GATE.replace(" || isFlashing()", ""))], "X6", ["X1"]),
    # Round-2 fixup, review issue 4: a reset writes only when there is something to zero.
    ("MR16", "every reset writes the file, 0 over 0 too", "gen_meter.inc",
     [(SKIP_ZERO, "  {")], "X7", ["X1"]),
    ("MR17", "the reset skips the write whenever RAM is 0 (the saved mark ignored)", "gen_meter.inc",
     [("dhwWaterTotalL != 0.0 || dhwMeterSavedL != 0.0", "dhwWaterTotalL != 0.0")], "X7", ["X1"]),
    # The reset and publish rules the round-2 review mutated in its scratchpad.
    ("MR18", "the consumer ignores the flag", "gen_meter.inc",
     [("  if (!dhwMeterResetPending) return;\n", "")], "Q2", ["Q1"]),
    ("MR19", "dhw_water_total published retained", "gen_disc_mqtt.inc",
     [('sendMQTTData(F("dhw_water_total"), msg, false);', 'sendMQTTData(F("dhw_water_total"), msg, true);')],
     "D7", ["D4"]),
    ("MR20", "sendDHWWaterTotal() ignores settings.mqtt.bEnable", "gen_disc_mqtt.inc",
     [("  if (!settings.mqtt.bEnable) return;\n  char msg[24];", "  char msg[24];")], "X5", ["X1"]),
    ("MR21", "the reset writes the file before it zeroes RAM", "gen_meter.inc",
     [(RESET_ZERO, "    writeDHWWaterMeterFile(millis());\n    dhwWaterTotalL = 0.0;\n")], "X1", ["D7"]),
    ("MR22", "no CORS header on the REST answer", "gen_rest_reset_branch.inc",
     [("    sendCorsOriginHeader();\n", "")], "Q1", ["Q2"]),
]

# Static wiring: (label, source, function anchor, call regex, [(must come after, regex)], [(must come before, regex)])
WIRING = [
    ("loadDHWWaterMeter() in setup()", FW_INO, r"^void setup\(\)\s*\{", r"^\s*loadDHWWaterMeter\(\);",
     [("the LittleFS mount", r"^\s*LittleFSmounted = LittleFS\.begin\(\);")],
     [("the first PIC or OT-Direct start", r"^\s*(detectPIC|initOTDirect|startPICSerialTask)\(\);")]),
    ("publishDHWWaterMeter() in doTaskEvery60s()", FW_INO, r"^void doTaskEvery60s\(\)\s*\{",
     r"^\s*publishDHWWaterMeter\(\);", [], []),
    ("saveDHWWaterMeterIfDue() in doTaskEvery60s()", FW_INO, r"^void doTaskEvery60s\(\)\s*\{",
     r"^\s*saveDHWWaterMeterIfDue\(millis\(\)\);", [], []),
    ("flushDHWWaterMeter() in doRestart()", HELPER, r"^void doRestart\(const char\* str\)\s*\{",
     r"^\s*flushDHWWaterMeter\(\);", [], [("platformRestart()", r"^\s*platformRestart\(\);")]),
    ("handlePendingDHWWaterMeterReset() in loop()", FW_INO, r"^void loop\(\)\s*$",
     r"^\s*handlePendingDHWWaterMeterReset\(\);",
     [("doBackgroundTasks(), where MQTT commands arrive", r"^\s*doBackgroundTasks\(\);")],
     [("drainOTFrameQueue()", r"^\s*drainOTFrameQueue\(\);"),
      ("the deferred reboot", r"^\s*if \(isRebootPending\(\) && !isFlashing\(\)\) performDeferredReboot\(\);")]),
    ("the otgw/ branch in handleMQTTcallback()", MQTT_INO,
     r"^static void handleMQTTcallback\(char\* topic, byte\* payload, unsigned int length(, bool retained)?\)\s*\{",
     r'^\s*if \(strcasecmp_P\(topicToken, PSTR\("otgw"\)\) == 0\) \{',
     [("the set/<node>/ match", r"^\s*if \(strcasecmp\(topicToken, NodeId\) == 0\) \{")],
     [("the OT command interface gate", r"^\s*if \(!hasOTCommandInterface\(\)\) \{")]),
    # The retained guard in the otgw/ branch is only live if the broker's retain flag
    # reaches it; anchored on the definition's last signature line (the forward
    # declaration shares the first one).
    ("the broker retain flag passed to handleMQTTcallback()", MQTT_INO,
     r"^\s*size_t len, size_t index, size_t total\) \{",
     r"^\s*handleMQTTcallback\(topicBuf, const_cast<byte\*>\(payload\), \(unsigned int\)len, properties\.retain\);",
     [], []),
    ("the reset_water_total route in handleOtgw()", REST_INO,
     r"^static void handleOtgw\(const char words\[\]\[API_WORD_LEN\], uint8_t wc, HTTPMethod method, "
     r"const char\* originalURI\) \{",
     r'^\s*\} else if \(strcmp_P\(words\[4\], PSTR\("reset_water_total"\)\) == 0\) \{', [], []),
]
FAUX_IDS = ["OTGWdallasdataid", "OTGWs0dataid", "OTGWheapstatsid", "OTGWpiccontrolsid", "OTGWfwinfoid",
            "OTGWpicinfoid", "OTGWpicsettingsid", "OTGWotdirectid", "OTGWdiag200id", "OTGWsatcoreid",
            "OTGWsatweatherid", "OTGWsatbinaryid", "OTGWsatzoneid", "OTGWhvacid", "OTGWdhwmeterid"]


def read(path, rev):
    if rev is None:
        return (REPO / path).read_text(encoding="utf-8", errors="replace")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], capture_output=True,
                          text=True, encoding="utf-8", errors="replace", check=True).stdout


def one(lines, rx):
    hits = [i for i, l in enumerate(lines) if re.search(rx, l)]
    if len(hits) != 1:
        raise SystemExit(f"anchor must match exactly once, got {len(hits)}: {rx}")
    return hits[0]


def construct(lines, rx):
    """The declaration starting at the one line matching rx, to its brace or ';'."""
    one(lines, rx)
    return slice_by_anchor(lines, rx)


def function_body(lines, rx):
    """The function whose signature line is the one line matching rx, through its closing
    brace at column 0. Unlike brace counting, a brace in a comment or string cannot end
    it early or late (handleOtgw() has '// root {' comments)."""
    start = one(lines, rx)
    end = next((i for i in range(start + 1, len(lines)) if re.match(r"^\}", lines[i])), None)
    if end is None:
        raise SystemExit(f"no closing brace at column 0 after: {rx}")
    return "\n".join(lines[start:end + 1])


def line(lines, rx):
    return lines[one(lines, rx)]


def typedef_ending(lines, name):
    end = one(lines, r"^\}\s*" + re.escape(name) + r"\s*;")
    start = next((i for i in range(end, -1, -1) if re.match(r"^typedef struct\s*\{", lines[i])), None)
    if start is None:
        raise SystemExit(f"no 'typedef struct {{' before }} {name};")
    return "\n".join(lines[start:end + 1])


def write(out_dir, name, origin, parts):
    head = f"// GENERATED by test_dhw_water_meter.py from {origin}. Do not edit.\n"
    (out_dir / name).write_text(head + "\n\n".join(parts) + "\n", encoding="utf-8")


METER_FUNCS = [
    r"void updateDHWWaterMeter\(float flowLitresPerMin, uint32_t nowMs\)",
    r"bool dhwWaterMeterHasData\(\)",
    r"bool dhwWaterMeterSaveDue\(uint32_t nowMs\)",
    r"static bool writeDHWWaterMeterFile\(uint32_t nowMs\)",
    r"void saveDHWWaterMeterIfDue\(uint32_t nowMs\)",
    r"void flushDHWWaterMeter\(\)",
    r"void loadDHWWaterMeter\(\)",
]
RESET_FUNCS = [
    r"void queueDHWWaterMeterReset\(\)",
    r"void handlePendingDHWWaterMeterReset\(\)",
]


def meter_slice():
    """dhwWaterMeter.ino: its constants and state, every function, the reset flag and the reset."""
    m = read(METER, None).splitlines()
    return [slice_range(m, r"^static const uint32_t DHW_METER_MAX_GAP_MS\b", r"^static uint32_t dhwMeterSavedMs\b"),
            *[construct(m, rf"^{sig}\s*$") for sig in METER_FUNCS],
            line(m, r"^static volatile bool dhwMeterResetPending\b"),
            *[construct(m, rf"^{sig}\s*$") for sig in RESET_FUNCS]]


def export_meter(core_rev, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    ch, hw = read(CORE_H, core_rev).splitlines(), read(HW_H, core_rev).splitlines()
    fwh, core = read(FW_H, core_rev).splitlines(), read(CORE, core_rev).splitlines()
    origin = f"git {core_rev}" if core_rev else "the working tree"
    write(out_dir, "gen_types.inc", origin, [
        construct(hw, r"^enum OTGWHardwareMode\b"),
        typedef_ending(ch, "OTdataStruct"),
        construct(ch, r"^enum OTLibMessageType\b"),
        construct(ch, r"^enum OTLibMessageID\b"),
        construct(ch, r"^\s*enum OTtype_t\b"),
        construct(ch, r"^\s*enum OTmsgcmd_t\b"),
        construct(ch, r"^\s*struct OTlookup_t\b"),
        construct(ch, r"^enum OTGW_response_type\b"),
        construct(ch, r"^struct OpenthermData_t\b"),
        construct(ch, r"^enum OTOverrideKind\b"),
    ])
    write(out_dir, "gen_fw_h.inc", origin, [
        construct(fwh, r"^inline bool isOTDirectEnabled\(\)"),
        construct(fwh, r"^inline bool isFlashing\(\)"),
    ])
    write(out_dir, "gen_meter.inc", "the working tree", meter_slice())
    write(out_dir, "gen_core.inc", origin, [
        construct(core, r"^float OpenthermData_t::f88\(\)"),
        construct(core, r"^void OpenthermData_t::f88\(float value\)"),
        *[slice_by_anchor(core, a) for a in RESERVED_ANCHORS],
        construct(core, r"^bool is_value_valid\(OpenthermData_t OT, OTlookup_t OTlookup\)\s*\{"),
        construct(core, r"^bool is_value_valid_for_master_topic\(OpenthermData_t OT, OTlookup_t OTlookup\)\s*\{"),
        construct(core, r"^void print_f88\(float& value\)\s*$"),
        construct(core, r"^static void stampThermostatRoomTemp\(\)\s*$"),
        construct(core, r"^static void updatePSSummaryFloatState\(uint8_t msgid, float fval\)\s*$"),
        construct(core, r"^static bool parseStrictFloat\(const char \*text, float &value\)\s*$"),
    ])


def rest_reset_branch(lines):
    """The body of handleOtgw()'s reset_water_total branch: the lines after its test, up to
    the next branch of that if-chain."""
    start = one(lines, r'^\s*\} else if \(strcmp_P\(words\[4\], PSTR\("reset_water_total"\)\) == 0\) \{\s*$')
    end = next((i for i in range(start + 1, len(lines))
                if re.match(r'^\s*\} else if \(strcmp_P\(words\[4\], PSTR\(', lines[i])), None)
    if end is None:
        raise SystemExit("no branch follows reset_water_total in handleOtgw()")
    return "\n".join(lines[start + 1:end])


def export_disc(out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    mh, disc = read(MQTT_H, None).splitlines(), read(DISC, None).splitlines()
    mino, fwh = read(MQTT_INO, None).splitlines(), read(FW_H, None).splitlines()
    rest = read(REST_INO, None).splitlines()
    origin = "the working tree"
    write(out_dir, "gen_disc_types.inc", origin, [
        *[construct(mh, rf"^enum class {e}\b") for e in
          ("HaDeviceClass", "HaUnit", "HaStateClass", "HaIcon", "HaEntityCat", "HaBinaryPayload")],
        slice_range(mh, r"^#ifndef MQTT_HA_FLAGS_DEFINED", r"^#endif"),
        construct(mh, r"^struct MqttHaSensorCfg\b"),
        construct(mh, r"^struct MqttHaBinSensorCfg\b"),
    ])
    write(out_dir, "gen_disc_table.inc", origin, [
        slice_range(disc, r"^// ========== Named PROGMEM strings: Labels",
                    r"^const uint16_t PROGMEM mqttHaBinSensorIndex\[256\]"),
    ])
    write(out_dir, "gen_disc_helpers.inc", origin, [
        construct(mh, r"^inline MqttHaSensorCfg readSensorCfg\("),
        construct(mh, r"^inline MqttHaBinSensorCfg readBinSensorCfg\("),
        construct(mh, r"^inline uint16_t readSensorIndex\("),
        construct(mh, r"^inline uint16_t readBinSensorIndex\("),
        line(mh, r"^constexpr uint16_t MQTT_HA_INDEX_NONE\b"),
        line(mh, r"^enum class HaDevice\b"),
        *[construct(disc, rf"^PGM_P {f}\(") for f in
          ("haDeviceClassStr", "haUnitStr", "haStateClassStr", "haIconStr", "haEntityCatStr")],
        construct(disc, r"^static HaDevice topoDeviceForPseudoId\(uint8_t otId\)\s*\{"),
    ])
    write(out_dir, "gen_disc_fw_h.inc", origin, [
        *[line(fwh, rf"^byte\s+{n}\s*=") for n in FAUX_IDS],
        line(fwh, r"^uint32_t\s+MQTTautoConfigMap\[8\]"),
        line(fwh, r"^uint32_t\s+MQTTautoCfgPendingMap\[8\]"),
        line(fwh, r"^void sendDHWWaterTotal\(\);"),
        line(fwh, r"^void publishDHWWaterMeter\(\);"),
        construct(fwh, r"^inline bool isFlashing\(\)"),
    ])
    write(out_dir, "gen_meter.inc", origin, meter_slice())
    write(out_dir, "gen_disc_mqtt.inc", origin, [
        line(mino, r"^static bool\s+dripDeviceInfoPending\b"),
        construct(mino, r"^static bool readMQTTTopicToken\(const char \*&cursor, char \*token, size_t tokenSize\) \{"),
        construct(mino, r"^bool getMQTTConfigDone\(const uint8_t MSGid\)\s*$"),
        construct(mino, r"^void setMQTTConfigDone\(const uint8_t MSGid\)\s*$"),
        construct(mino, r"^void clearMQTTConfigDone\(\)\s*$"),
        construct(mino, r"^void clearMQTTConfigPending\(\)\s*$"),
        construct(mino, r"^static void queueNonOTDiscoveryIds\(\)\s*$"),
        construct(mino, r"^void publishNonOTDiscoveryConfigs\(\)\s*$"),
        construct(mino, r"^void setMQTTConfigPending\(const uint8_t MSGid\)\s*$"),
        construct(mino, r"^void markAllMQTTConfigPending\(\)\s*$"),
        construct(mino, r"^static HaDevice deviceForOTId\(byte OTid\)\s*\{"),
        construct(mino, r"^void sendDHWWaterTotal\(\)\s*$"),
        construct(mino, r"^void publishDHWWaterMeter\(\)\s*$"),
    ])
    write(out_dir, "gen_rest_reset_branch.inc", origin, [rest_reset_branch(rest)])
    write(out_dir, "gen_mqtt_otgw_branch.inc", origin, [
        construct(mino, r'^\s*if \(strcasecmp_P\(topicToken, PSTR\("otgw"\)\) == 0\) \{\s*$'),
    ])


def mutate(out_dir, fname, subs):
    path = out_dir / fname
    text = path.read_text(encoding="utf-8")
    for old, new in subs:
        n = text.count(old)
        if n != 1:
            raise SystemExit(f"mutant anchor must match exactly once in {fname}, got {n}: {old!r}")
        mutated = text.replace(old, new)
        if mutated == text:
            raise SystemExit(f"mutant left {fname} unchanged")
        text = mutated
    path.write_text(text, encoding="utf-8")


def build_and_run(label, out_dir, cpp):
    exe = out_dir / (Path(cpp).stem + ".exe")
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / cpp}\" /I\"{out_dir}\" /I\"{HERE}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-4000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    cases = dict(re.findall(r"^CASE (\S+) (pass|FAIL)\b", r.stdout, re.M))
    return r.returncode, r.stdout, cases


def run_mutants(root, mutants, exporter, cpp, checks):
    for mid, what, fname, subs, case, keeps, *rest in mutants:
        crash_ok = bool(rest and rest[0])
        mdir = root / f"mutant_{mid}"
        exporter(mdir)
        mutate(mdir, fname, subs)
        rc, out, cases = build_and_run(f"{mid} {what}", mdir, cpp)
        failed = sorted(k for k, v in cases.items() if v == "FAIL")
        ended = "" if rc in (0, 1) else f"; process ended with exit code 0x{rc & 0xFFFFFFFF:08X} before {case} reported"
        print(f"== {mid}: {what} -> failed cases: {', '.join(failed) or 'none'}{ended}")
        for m in re.finditer(rf"^CASE {case} .*\n.*\n.*$", out, re.M):
            print("   " + m.group(0).replace("\n", "\n   "))
        if crash_ok:
            killed = cases.get(case) != "pass" and rc != 0
        else:
            killed = cases.get(case) == "FAIL" and rc == 1
        checks.append((f"{mid} ({what}) fails {case}", killed))
        if keeps:
            checks.append((f"{mid} leaves {','.join(keeps)} passing", all(cases.get(k) == "pass" for k in keeps)))


def wiring_check(body, call_rx, after, before):
    """(ok, text) for one call in one function body."""
    lines = body.splitlines()
    hits = [i for i, l in enumerate(lines) if re.search(call_rx, l)]
    if len(hits) != 1:
        return False, f"{len(hits)} call statements (want 1)"
    at = hits[0]
    notes = [f"body line {at + 1}"]
    ok = True
    for what, rx in after:
        prior = [i for i, l in enumerate(lines) if re.search(rx, l)]
        good = bool(prior) and min(prior) < at
        ok &= good
        notes.append(f"after {what}: {'yes' if good else 'NO'}")
    for what, rx in before:
        later = [i for i, l in enumerate(lines) if re.search(rx, l)]
        good = bool(later) and at < min(later)
        ok &= good
        notes.append(f"before {what}: {'yes' if good else 'NO'}")
    return ok, ", ".join(notes)


def wiring_report(rev):
    """Each WIRING entry checked in the function body taken from the sources at rev."""
    out = []
    for label, src, fn_rx, call_rx, after, before in WIRING:
        body = function_body(read(src, rev).splitlines(), fn_rx)
        ok, text = wiring_check(body, call_rx, after, before)
        out.append((label, body, call_rx, after, before, ok, text))
    return out


def route_table_check(text):
    """(ok, text): /api/v2/otgw/* reaches handleOtgw() through kV2Routes (restAPI.ino)."""
    name = re.findall(r'^static const char kRouteOtgw\[\]\s+PROGMEM = "otgw";', text, re.M)
    row = re.findall(r"^\s*\{ kRouteOtgw,\s+handleOtgw \},", text, re.M)
    return len(name) == 1 and len(row) == 1, f"kRouteOtgw = \"otgw\": {len(name)}, row {{ kRouteOtgw, handleOtgw }}: {len(row)}"


REQUEUE_RX = r"^\s*(publishNonOTDiscoveryConfigs|queueNonOTDiscoveryIds)\(\);"


def firmware_sources():
    """(file name, lines) of every .ino and .cpp in the firmware directory."""
    paths = sorted((REPO / FW).glob("*.ino")) + sorted((REPO / FW).glob("*.cpp"))
    return [(p.name, p.read_text(encoding="utf-8", errors="replace").splitlines()) for p in paths]


def requeue_report(sources):
    """Every clearMQTTConfigDone() call statement, and whether the same function queues
    the non-OT ids again after it (queueNonOTDiscoveryIds() holds 241)."""
    out = []
    for name, lines in sources:
        for i, l in enumerate(lines):
            if not re.match(r"^\s+clearMQTTConfigDone\(\);", l):
                continue
            sig = next((j for j in range(i, -1, -1) if re.match(r"^[A-Za-z_][^;]*\(", lines[j])), None)
            end = next((j for j in range(i, len(lines)) if re.match(r"^\}", lines[j])), len(lines))
            fn = re.search(r"(\w+)\s*\(", lines[sig]).group(1) if sig is not None else "?"
            ok = any(re.match(REQUEUE_RX, lines[j]) for j in range(i + 1, end))
            out.append((f"{name} {fn}() line {i + 1}", ok))
    return out


def without_first_requeue(sources):
    """The sources with the re-queue call after the first clearMQTTConfigDone() call removed."""
    out, done = [], False
    for name, lines in sources:
        lines = list(lines)
        if not done:
            call = next((i for i, l in enumerate(lines) if re.match(r"^\s+clearMQTTConfigDone\(\);", l)), None)
            if call is not None:
                cut = next(i for i in range(call + 1, len(lines)) if re.match(REQUEUE_RX, lines[i]))
                del lines[cut]
                done = True
        out.append((name, lines))
    return out


def old_discovery_facts(rev):
    """What the OLD revision has for the entity, read from its sources (not compiled)."""
    disc, mino = read(DISC, rev), read(MQTT_INO, rev)
    idx = re.search(r"^\s*(0x[0-9A-Fa-f]+|\d+),\s*// id 241\b", disc, re.M)
    return (f"'dhw_water_total' in MQTTHaDiscovery.cpp: {'dhw_water_total' in disc}; "
            f"in MQTTstuff.ino: {'dhw_water_total' in mino}; mqttHaSensorIndex[241] = {idx.group(1) if idx else '?'}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--old-rev", help="also build the write sites from this git revision (OLD)")
    args = ap.parse_args()
    root = GEN / "dhw_water_meter"
    checks = []

    fix_dir = root / "fix"
    export_meter(None, fix_dir)
    fix_rc, fix_out, fix_cases = build_and_run("FIX (working tree)", fix_dir, "test_dhw_water_meter.cpp")
    print("== FIX (working tree): accumulator, write sites, persistence ==")
    print(fix_out, end="")
    checks.append(("FIX passes every meter case", fix_rc == 0 and sorted(fix_cases) == sorted(METER_CASES)
                   and all(v == "pass" for v in fix_cases.values())))

    print("== FIX (working tree): wiring, checked in the function bodies ==")
    for label, body, call_rx, after, before, ok, text in wiring_report(None):
        print(f"  {label}: {'present' if ok else 'MISSING OR MISPLACED'} ({text})")
        checks.append((f"FIX wiring: {label}", ok))
        # The check itself must be able to fail: drop the call and look again.
        cut = "\n".join(l for l in body.splitlines() if not re.search(call_rx, l))
        flipped = not wiring_check(cut, call_rx, after, before)[0]
        checks.append((f"wiring check flips when {label.split(' in ')[0]} is removed", flipped))
    rest_text = read(REST_INO, None)
    ok, text = route_table_check(rest_text)
    print(f"  REST route table: {'present' if ok else 'MISSING'} ({text})")
    checks.append(("FIX wiring: /api/v2/otgw/* reaches handleOtgw()", ok))
    cut = re.sub(r"^\s*\{ kRouteOtgw,\s+handleOtgw \},\n", "", rest_text, flags=re.M)
    checks.append(("route table check flips when the otgw row is removed", not route_table_check(cut)[0]))

    print("== FIX (working tree): every clearMQTTConfigDone() call queues the non-OT ids again ==")
    sources = firmware_sources()
    requeue = requeue_report(sources)
    for where, ok in requeue:
        print(f"  {where}: {'re-queues' if ok else 'DOES NOT re-queue'}")
    checks.append(("every clearMQTTConfigDone() call is followed by a non-OT re-queue",
                   bool(requeue) and all(ok for _, ok in requeue)))
    flipped = requeue_report(without_first_requeue(sources))
    checks.append(("re-queue check flips when the first re-queue call is removed",
                   len(flipped) == len(requeue) and sum(not ok for _, ok in flipped) == 1))

    if args.old_rev:
        old_dir = root / "old"
        export_meter(args.old_rev, old_dir)
        old_rc, old_out, old_cases = build_and_run(f"OLD (git {args.old_rev})", old_dir, "test_dhw_water_meter.cpp")
        print(f"== OLD (OTGW-Core.ino and headers at git {args.old_rev}; same driver and meter module) ==")
        print(old_out, end="")
        checks.append(("OLD fails exactly the write-site cases " + ",".join(OLD_DEFECT),
                       sorted(k for k, v in old_cases.items() if v == "FAIL") == sorted(OLD_DEFECT)))
        checks.append(("OLD passes every other case", sorted(old_cases) == sorted(METER_CASES)))
        checks.append(("OLD exits 1", old_rc == 1))
        print(f"== OLD discovery and wiring, from the git {args.old_rev} sources ==")
        print("  " + old_discovery_facts(args.old_rev))
        old_wiring = wiring_report(args.old_rev)
        for label, _, _, _, _, ok, text in old_wiring:
            print(f"  {label}: {'present' if ok else 'absent'} ({text})")
        checks.append(("OLD has none of the wiring", not any(w[5] for w in old_wiring)))

    run_mutants(root, METER_MUTANTS, lambda d: export_meter(None, d), "test_dhw_water_meter.cpp", checks)

    disc_dir = root / "disc_fix"
    export_disc(disc_dir)
    d_rc, d_out, d_cases = build_and_run("discovery FIX", disc_dir, "test_dhw_water_discovery.cpp")
    print("== FIX (working tree): discovery, publish and reset ==")
    print(d_out, end="")
    checks.append(("FIX passes every discovery, publish and reset case", d_rc == 0 and sorted(d_cases) == sorted(DISC_CASES)
                   and all(v == "pass" for v in d_cases.values())))

    run_mutants(root, DISC_MUTANTS, export_disc, "test_dhw_water_discovery.cpp", checks)

    print("== verdict ==")
    for text, ok in checks:
        print(f"  {text}: {'yes' if ok else 'NO'}")
    ok = all(ok for _, ok in checks)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
