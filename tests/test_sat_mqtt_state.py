#!/usr/bin/env python3
"""TASK-1284: source-level checks for the SAT MQTT state topics.

These are NOT behaviour tests: they read the firmware sources and check that the pieces are
wired together (disable and trip paths publish the disabled state, retained flags, the new
command, the HA discovery topics, the docs table). The topics and payloads themselves are
verified on the bench with a broker capture.

OTGW_SRC_REV=<git rev> reads every source file from that revision instead of the working tree
(old-vs-fix run: `OTGW_SRC_REV=HEAD python tests/test_sat_mqtt_state.py` goes red on the commit
before TASK-1284).

Run: python tests/test_sat_mqtt_state.py
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"
REV = os.environ.get("OTGW_SRC_REV")


def read(rel, base="src/OTGW-firmware"):
    if REV:
        out = subprocess.run(["git", "show", f"{REV}:{base}/{rel}"], cwd=REPO, capture_output=True, check=True)
        return out.stdout.decode("utf-8")
    return (REPO / base / rel).read_text(encoding="utf-8")


def body(src, signature):
    """Text of the function whose definition starts with `signature`, brace-matched."""
    i = -1
    while True:  # skip forward declarations: the definition is the one followed by "{"
        i = src.index(signature, i + 1)
        rest = src[i + len(signature):i + len(signature) + 40].lstrip()
        if rest.startswith("{"):
            break
    j = src.index("{", i)
    depth, k = 0, j
    while True:
        c = src[k]
        depth += c == "{"
        depth -= c == "}"
        k += 1
        if depth == 0:
            return src[j:k]


def core_shadow_names(sat):
    """Names of the file-scope s_core_* shadows declared before satPublishCoreState()'s definition."""
    start = sat.index("static SATShadowB s_core_enabled")
    end = sat.index("static void satPublishCoreState()" + chr(10) + "{")      # the definition, not a prototype
    names = []
    for decl in re.findall(r"static SAT\w+ ([^;]+);", sat[start:end]):
        names += [n.strip() for n in decl.split(",")]
    return names


def missing_resets(sat):
    """Shadows declared but not memset in satRepublishCoreState()."""
    reset = body(sat, "void satRepublishCoreState()")
    return [n for n in core_shadow_names(sat) if "&" + n + "," not in reset]


class DisabledStateIsPublished(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sat = read("SATcontrol.ino")

    def test_core_state_topics_are_retained(self):
        self.assertIn("satPublishCoreState", self.sat)
        core = body(self.sat, "static void satPublishCoreState()")
        for topic in ("enabled", "active", "safety_tripped", "fallback_active", "external_temp_enable",
                      "mode", "control_mode", "preset"):
            with self.subTest(topic):
                self.assertRegex(core, r'publishIfChanged[BS]\(F\("sat/%s"\),[^;]*,\s*true\);' % topic)

    def test_both_trip_paths_set_the_flag_then_disable(self):
        # TASK-1364: the lost-room-temperature trip is gone (curve only); the PIC-fail trip is the one left.
        sites = [m.start() for m in re.finditer(r"state\.sat\.bSafetyTripped = true;", self.sat)]
        self.assertEqual(len(sites), 1)
        for s in sites:
            with self.subTest(offset=s):
                self.assertIn("satDisable();", self.sat[s:s + 200])

    # --- B: publish on the loop task only ---------------------------------------------------
    def test_async_reachable_functions_do_not_publish(self):
        # satDisable()/satClearSafetyTrip() are reached from the async_tcp task (REST, settings POST)
        self.assertNotIn("satPublishCoreState", body(self.sat, "void satDisable(bool persistPid)"))
        self.assertNotIn("satPublishCoreState", body(self.sat, "void satClearSafetyTrip()"))
        self.assertNotIn("satPublishCoreState", body(self.sat, "void satHandleEnabled(const char* value)"))

    def test_only_the_control_loop_wrapper_publishes_the_core_state(self):
        calls = 0
        for f in SRC.glob("*"):
            if f.suffix in (".ino", ".cpp", ".h"):
                text = f.read_text(encoding="utf-8", errors="replace") if not REV else (read(f.name) if f.name == "SATcontrol.ino" else "")
                calls += len(re.findall(r"^\s*satPublishCoreState\(\);", text, re.M))
        self.assertEqual(calls, 1)
        loop = body(self.sat, "void satControlLoop()")
        self.assertRegex(loop, r"satControlLoopRun\(\);\s*satPublishCoreState\(\);")
        # every path of the pass (enabled, disabled, tripped) goes through the wrapper
        self.assertNotIn("satPublishCoreState", body(self.sat, "static void satControlLoopRun()"))
        self.assertIn("satControlLoop();", read("OTGW-firmware.ino"))

    def test_enabled_path_does_not_publish_the_core_topics_again(self):
        pub = body(self.sat, "void satPublishMQTT()")
        self.assertNotIn("satPublishCoreState", pub)
        for topic in ("sat/mode", "sat/active", "sat/safety_tripped", "sat/external_temp_enable"):
            self.assertNotIn('F("%s")' % topic, pub)

    # --- shadow reset (F) -------------------------------------------------------------------
    def test_republish_all_resets_the_core_shadows(self):
        core = read("OTGW-Core.ino")
        self.assertIn("satRepublishCoreState();", body(core, "void requestMQTTRepublishAll()"))
        names = core_shadow_names(self.sat)
        self.assertGreaterEqual(len(names), 8)          # 5 bool + 3 string shadows
        self.assertEqual(missing_resets(self.sat), [])

    def test_shadow_reset_check_catches_a_removed_memset(self):
        mutated = re.sub(r"  memset\(&s_core_mode,[^\n]*\n", "", self.sat)
        self.assertNotEqual(mutated, self.sat)
        self.assertEqual(missing_resets(mutated), ["s_core_mode"])

    # --- A: sat/mode is off while disabled or tripped ---------------------------------------
    def test_mode_topic_is_off_while_disabled_or_tripped(self):
        core = body(self.sat, "static void satPublishCoreState()")
        guard = core.index("if (!satIsEnabled() || state.sat.bSafetyTripped) modeIdx = 0;")
        self.assertLess(guard, core.index('F("sat/mode")'))

    def test_control_mode_command_keeps_the_actual_mode_while_disabled_or_tripped(self):
        h = body(self.sat, "void satHandleControlMode(const char* value)")
        self.assertIn("const bool live = satIsEnabled() && !state.sat.bSafetyTripped;", h)
        calls = re.findall(r"^\s*(if \(live\) )?satSetControlMode\(", h, re.M)
        self.assertEqual(len(calls), 3)
        self.assertTrue(all(calls), "an unguarded satSetControlMode() call changes eControlMode while disabled")
        for n in "012":
            self.assertIn("state.sat.iRequestedControlMode = %s;" % n, h)
        self.assertIn("iRequestedControlMode", read("SATtypes.h"))


class ExternalTempSetting(unittest.TestCase):
    def test_wired(self):
        self.assertRegex(read("MQTTstuff.ino"), r'\{\s*"external_temp",\s*"SATexternaltemp",\s*nullptr\s*\}')
        self.assertIn('PSTR("external_temp")', read("restAPI.ino"))
        ha = read("MQTTHaDiscovery.cpp")
        self.assertIn("case 2:", ha)   # moved into the slot of the removed comfort adjust switch (TASK-1282)
        self.assertNotIn("case 13:", ha)
        self.assertIn('PSTR("/sat/external_temp"),           PSTR("/sat/external_temp_enable")', ha)
        # The discovery loop must reach the last switch (TASK-1287: the bound is SAT_HA_SWITCH_COUNT).
        self.assertRegex(read("MQTTstuff.ino"), r"swIdx < SAT_HA_SWITCH_COUNT")
        count = re.search(r"constexpr uint8_t SAT_HA_SWITCH_COUNT = (\d+);", read("MQTTstuff.h"))
        self.assertIsNotNone(count)
        self.assertEqual(int(count.group(1)), 13)


class PvBoostDiscoveryMatchesFirmware(unittest.TestCase):
    def test_switch_and_number_topics_use_pv_prefix(self):
        m = read("MQTTstuff.ino")
        self.assertNotIn('\\"stat_t\\":\\"%s/sat/%s\\",\\"cmd_t\\"', m)
        self.assertEqual(m.count('\\"stat_t\\":\\"%s/sat/pv_%s\\",\\"cmd_t\\":\\"%s/sat/pv_%s\\"'), 2)

    def test_every_discovered_pv_metric_exists_as_state_and_command(self):
        m, sat = read("MQTTstuff.ino"), read("SATcontrol.ino")
        metrics = re.findall(r'publish(?:Switch|Number)\("(boost_\w+)"', m)
        self.assertEqual(len(metrics), 6)
        for metric in metrics:
            with self.subTest(metric):
                self.assertIn('F("sat/pv_%s")' % metric, sat)                       # state
                self.assertRegex(m, r'\{\s*"pv_%s",' % metric)                     # command
        # sensors and the binary sensor already used sat/pv_<metric>
        for metric in re.findall(r'publish(?:Sensor|Binary)\("(\w+)"', m):
            with self.subTest(metric):
                self.assertIn('F("sat/pv_%s")' % metric, sat)


class DocsMatchTheCommandTable(unittest.TestCase):
    def test_manual_passthrough_table_equals_kSatMqttCmds(self):
        m = read("MQTTstuff.ino")
        table = m[m.index("kSatMqttCmds[] PROGMEM"):]
        table = table[:table.index("{ nullptr, nullptr, nullptr }")]
        code = {(c, k) for c, k in re.findall(r'\{\s*"(\w+)",\s*"(SAT\w+)",\s*nullptr\s*\}', table)}
        manual = read("MANUAL.md", "docs")
        start = manual.index("The commands below pass the payload unchanged")
        end = manual.index("\n\n", manual.index("| Topic suffix | Setting |", start))
        doc = {(c, k) for c, k in re.findall(r"\| `sat/(\w+)` \| `(SAT\w+)` \|", manual[start:end])}
        self.assertEqual(code - doc, set(), "in code, missing in MANUAL.md")
        self.assertEqual(doc - code, set(), "in MANUAL.md, missing in code")


# --- TASK-1339: outbox gate in the ADR-111 helpers ---------------------------------------------
# Behaviour test of the REAL SATmqttPublish.h + SATmqttPublish.cpp (include lines stripped),
# compiled with a host C++ compiler next to stubs for millis(), dtostrf(), sendMQTTData() and the
# outbox depth. OTGW_SRC_REV=HEAD runs the same scenarios against the code before the gate: the
# publish-count and shadow assertions go red there (the deferred flag does not exist on HEAD and
# is reported as -1, so only the flag assertions read it).

GATE_PRELUDE = r"""
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define snprintf_P snprintf
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
static char* dtostrf(double v, signed char w, unsigned char p, char* b) { sprintf(b, "%*.*f", w, p, v); return b; }
static size_t test_strlcpy(char* d, const char* s, size_t n) {
  size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
#define strlcpy test_strlcpy
static uint32_t platformHardwareRandom() { return 12345u; }
static uint32_t g_outbox = 0;
static int g_sent = 0;
static char g_lastPayload[64];
uint32_t mqttOutboxDepth() { return g_outbox; }
bool sendMQTTData(const char* topic, const char* json, const bool retain) {
  (void)topic; (void)retain; ++g_sent; ++g_outbox; test_strlcpy(g_lastPayload, json, sizeof(g_lastPayload)); return true; }
bool sendMQTTData(const __FlashStringHelper* topic, const char* json, const bool retain) {
  return sendMQTTData(reinterpret_cast<const char*>(topic), json, retain); }
"""

GATE_MAIN = r"""
#include <string>
int main() {
  // 1. float, first-seen, outbox above the limit, then drained
  SATShadowF f{};
  g_outbox = 1000; g_sent = 0; DEFERRED_CLEAR();
  bool r = publishIfChangedF(F("sat/setpoint"), 21.5f, f, 0.05f, 1, true);
  printf("f_full r=%d sent=%d valid=%d next=%u deferred=%d\n", (int)r, g_sent, (int)f.valid, (unsigned)f.nextRepublishMs, DEFERRED());
  g_outbox = 0; g_sent = 0; DEFERRED_CLEAR();
  r = publishIfChangedF(F("sat/setpoint"), 21.5f, f, 0.05f, 1, true);
  printf("f_free r=%d sent=%d valid=%d payload=%s deferred=%d\n", (int)r, g_sent, (int)f.valid, g_lastPayload, DEFERRED());
  // 2. a quiet call (nothing due) with a full outbox does not set the flag
  g_outbox = 1000; g_sent = 0; DEFERRED_CLEAR();
  r = publishIfChangedF(F("sat/setpoint"), 21.5f, f, 0.05f, 1, true);
  printf("f_quiet r=%d sent=%d deferred=%d\n", (int)r, g_sent, DEFERRED());
  // 3. float changed while full: shadow keeps the old value
  r = publishIfChangedF(F("sat/setpoint"), 23.0f, f, 0.05f, 1, true);
  printf("f_changed_full r=%d sent=%d last=%.1f deferred=%d\n", (int)r, g_sent, f.last, DEFERRED());
  // 4. bool first-seen while full: nextRepublishMs stays 0 (still first-seen)
  SATShadowB b{};
  g_outbox = 1000; g_sent = 0; DEFERRED_CLEAR();
  r = publishIfChangedB(F("sat/active"), true, b, true);
  printf("b_full r=%d sent=%d next=%u\n", (int)r, g_sent, (unsigned)b.nextRepublishMs);
  g_outbox = 0;
  r = publishIfChangedB(F("sat/active"), true, b, true);
  printf("b_free r=%d sent=%d payload=%s\n", (int)r, g_sent, g_lastPayload);
  // 5. string, char* topic, first-seen while full
  SATShadowS s{};
  g_outbox = 1000; g_sent = 0;
  r = publishIfChangedS("sat/ble/aabb/x", "HEALTHY", s, false);
  printf("s_full r=%d sent=%d valid=%d last=[%s]\n", (int)r, g_sent, (int)s.valid, s.last);
  // 6. boundary: exactly at the limit still publishes
  SATShadowI i{};
  g_outbox = 24; g_sent = 0;
  r = publishIfChangedI(F("sat/max_modulation"), 100, i, true);
  printf("i_at24 r=%d sent=%d\n", (int)r, g_sent);
  // 7. JSON blob: change signal while full is kept as "due now"
  uint32_t hb = 0;
  g_outbox = 0; g_sent = 0;
  publishJsonAttrIfChanged(F("sat/pid_attributes"), "{}", hb, false, false);   // first publish
  g_now += 5000;
  g_outbox = 1000; g_sent = 0;
  r = publishJsonAttrIfChanged(F("sat/pid_attributes"), "{\"a\":1}", hb, true, false);
  printf("blob_full r=%d sent=%d due=%d\n", (int)r, g_sent, (int)((int32_t)(millis() - hb) >= 0));
  g_now += 500; g_outbox = 0; g_sent = 0;
  r = publishJsonAttrIfChanged(F("sat/pid_attributes"), "{\"a\":1}", hb, false, false);   // source shadows already updated
  printf("blob_next r=%d sent=%d\n", (int)r, g_sent);
  return 0;
}
"""


def strip_includes(text):
    return re.sub(r'^\s*#include\s*[<"][^>"]+[>"][^\n]*$', "", text, flags=re.M)


class OutboxGate(unittest.TestCase):
    """TASK-1339: a due publish is skipped (shadow untouched, flag set) while the outbox is full."""

    @classmethod
    def setUpClass(cls):
        import sys
        import tempfile
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from test_sat_published_curve import compile_cpp
        h, cpp = read("SATmqttPublish.h"), read("SATmqttPublish.cpp")
        has_flag = "satPublishDeferred" in h
        flag = ("#define DEFERRED() ((int)satPublishDeferred())\n#define DEFERRED_CLEAR() satPublishDeferredClear()\n"
                if has_flag else "#define DEFERRED() (-1)\n#define DEFERRED_CLEAR() ((void)0)\n")
        cls.tmp = tempfile.TemporaryDirectory()
        src = Path(cls.tmp.name) / "gate.cpp"
        src.write_text(GATE_PRELUDE + strip_includes(h) + "\n" + strip_includes(cpp) + "\n" + flag + GATE_MAIN,
                       encoding="utf-8")
        cls.exe = compile_cpp(src, Path(cls.tmp.name) / "gate.exe")
        cls.out = {}
        if cls.exe:
            text = subprocess.run([str(cls.exe)], capture_output=True, text=True, check=True).stdout
            print("\n" + text)
            for line in text.splitlines():
                name, *kv = line.split()
                cls.out[name] = dict(p.split("=", 1) for p in kv)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def r(self, name):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return self.out[name]

    def test_full_outbox_skips_and_keeps_shadow_first_seen(self):
        o = self.r("f_full")
        self.assertEqual((o["r"], o["sent"], o["valid"], o["next"]), ("0", "0", "0", "0"))
        self.assertEqual(o["deferred"], "1")

    def test_drained_outbox_publishes_the_skipped_topic(self):
        o = self.r("f_free")
        self.assertEqual((o["r"], o["sent"], o["valid"], o["payload"]), ("1", "1", "1", "21.5"))
        self.assertEqual(o["deferred"], "0")

    def test_quiet_call_does_not_set_the_flag(self):
        o = self.r("f_quiet")
        self.assertEqual((o["r"], o["sent"], o["deferred"]), ("0", "0", "0"))

    def test_changed_value_is_not_written_to_the_shadow_while_full(self):
        o = self.r("f_changed_full")
        self.assertEqual((o["r"], o["sent"], o["last"]), ("0", "0", "21.5"))
        self.assertEqual(o["deferred"], "1")

    def test_bool_stays_first_seen_while_full(self):
        self.assertEqual(self.r("b_full"), {"r": "0", "sent": "0", "next": "0"})
        self.assertEqual(self.r("b_free"), {"r": "1", "sent": "1", "payload": "true"})

    def test_char_topic_overload_is_gated(self):
        self.assertEqual(self.r("s_full"), {"r": "0", "sent": "0", "valid": "0", "last": "[]"})

    def test_at_the_limit_still_publishes(self):
        self.assertEqual(self.r("i_at24"), {"r": "1", "sent": "1"})

    def test_blob_change_signal_survives_a_skip(self):
        self.assertEqual(self.r("blob_full"), {"r": "0", "sent": "0", "due": "1"})
        self.assertEqual(self.r("blob_next"), {"r": "1", "sent": "1"})

    # --- source wiring -----------------------------------------------------------------------
    def test_every_helper_is_gated_before_its_shadow_write(self):
        cpp = read("SATmqttPublish.cpp")
        defs = re.findall(r"^bool publish\w+\([^)]*\)\s*\{", cpp, re.M)
        self.assertEqual(len(defs), 10)
        for d in defs:
            with self.subTest(d.split("(")[0]):
                fn = body(cpp, d[:-1].rstrip())
                self.assertIn("satOutboxFull()", fn)
                self.assertLess(fn.index("satOutboxFull()"), fn.index("sendMQTTData("))
                write = re.search(r"shadow\.\w+\s*=[^=]|strlcpy\(shadow|nextRepublishMs = satNextDeadline", fn)
                self.assertIsNotNone(write)
                self.assertLess(fn.index("satOutboxFull()"), write.start())

    def test_limit_is_a_named_constant(self):
        self.assertRegex(read("SATmqttPublish.h"),
                         r"static constexpr uint32_t SAT_MQTT_OUTBOX_LIMIT = 24;\s*// TASK-1339")

    def test_publish_pass_clears_the_flag_before_its_gates(self):
        pub = body(read("SATcontrol.ino"), "void satPublishMQTT()")
        gate = pub.index("if (!settings.mqtt.bEnable")
        self.assertLess(pub.index("satPublishDeferredClear();"), gate)
        self.assertLess(pub.index("_sat_publishRetryPending = false;"), gate)

    def test_retry_follows_only_a_sat_pass_that_skipped(self):
        # The retry reads a per-pass flag set after the last publish of satPublishMQTT(), not the
        # shared helper flag: a core-state skip must not start a SAT publish before a control tick.
        pub = body(read("SATcontrol.ino"), "void satPublishMQTT()")
        last = pub.index("_sat_publishRetryPending = satPublishDeferred();")
        self.assertLess(pub.rindex("satBLEPublishMQTT();"), last)
        tail = pub[pub.index(";", last) + 1:]
        self.assertIsNone(re.search(r"[Pp]ublish\w*\(", tail), "a publish after the flag is read escapes the retry")
        run = body(read("SATcontrol.ino"), "static void satControlLoopRun()")
        self.assertNotIn("satPublishDeferred()", run)

    def test_retry_runs_before_the_trip_and_ot_loss_returns(self):
        # Bench 2026-10-26 (alpha.511): with the retry after satOtLinkStep(), a unit without a
        # boiler published ~33 of ~115 sat/* topics after boot and never the rest. Publishing
        # sends nothing to the boiler, so the retry runs after the disabled/flash bail only.
        run = body(read("SATcontrol.ino"), "static void satControlLoopRun()")
        retry = run.index("if (_sat_publishRetryPending && DUE(timerSATPublishRetry)) satPublishMQTT();")
        self.assertLess(run.index("if (!satIsEnabled() || isFlashing()) {"), retry)
        for later in ("if (state.sat.bSafetyTripped) return;", "if (satOtLinkStep()) return;",
                      "if (!DUE(timerSATControl)) return;"):
            self.assertLess(retry, run.index(later))
        self.assertIn("DECLARE_TIMER_MS(timerSATPublishRetry, 500, SKIP_MISSED_TICKS);", read("SATcontrol.ino"))


if __name__ == "__main__":
    unittest.main()
