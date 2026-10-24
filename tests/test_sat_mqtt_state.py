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
        sites = [m.start() for m in re.finditer(r"state\.sat\.bSafetyTripped = true;", self.sat)]
        self.assertEqual(len(sites), 2)
        for s in sites:
            with self.subTest(offset=s):
                self.assertIn("satDisable();", self.sat[s:s + 200])

    # --- B: publish on the loop task only ---------------------------------------------------
    def test_async_reachable_functions_do_not_publish(self):
        # satDisable()/satClearSafetyTrip() are reached from the async_tcp task (REST, settings POST)
        self.assertNotIn("satPublishCoreState", body(self.sat, "void satDisable()"))
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
        self.assertIn("case 14:", ha)
        self.assertIn('PSTR("/sat/external_temp"),           PSTR("/sat/external_temp_enable")', ha)
        # The discovery loop must reach switch 14 (TASK-1287: the bound is SAT_HA_SWITCH_COUNT).
        self.assertRegex(read("MQTTstuff.ino"), r"swIdx < SAT_HA_SWITCH_COUNT")
        count = re.search(r"constexpr uint8_t SAT_HA_SWITCH_COUNT = (\d+);", read("MQTTstuff.h"))
        self.assertIsNotNone(count)
        self.assertGreaterEqual(int(count.group(1)), 15)


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


if __name__ == "__main__":
    unittest.main()
