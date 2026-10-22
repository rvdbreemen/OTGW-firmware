#!/usr/bin/env python3
"""TASK-1294 guard: the built-in OpenTherm replay fixture.

/otgw_simulation.log is no longer a file in the LittleFS image. The fixture is
compiled into the firmware (otgwSimulationFixture.h, PROGMEM) and the replay in
OTGW-Core.ino falls back to it when LittleFS holds no /otgw_simulation.log. An
uploaded file keeps precedence.

What this test checks:
  1. The header's lines equal the canonical list in
     tests/fixtures/otgw_simulation.builtin.log (the former data file), in order.
  2. data/otgw_simulation.log is gone from the image sources.
  3. Source level, OTGW-Core.ino: the 'simulation disabled ... not found' path is
     gone; reopenOTGWSimulationFile() prefers the LittleFS file and otherwise
     selects the built-in source; nextOTGWSimulationByte() reads the PROGMEM
     fixture with pgm_read_byte(); handlePICSerialSimulation() decides the source
     from the explicit source state (not from File truthiness, which stays false
     for the built-in source).
  4. Source level, restAPI.ino: the /api/v2/simulate JSON always says
     available:true and carries "source" ("file" or "builtin").

What it does NOT check: that the firmware compiles, or that the replay emits
the frames on a device. Those need the build and the bench. The checks in 3 and
4 read the source text; they cannot prove runtime behaviour.

Run: python tests/test_sim_fixture_builtin.py
"""
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "src" / "OTGW-firmware"
HEADER = FW / "otgwSimulationFixture.h"
CORE = FW / "OTGW-Core.ino"
RESTAPI = FW / "restAPI.ino"
CANON = REPO / "tests" / "fixtures" / "otgw_simulation.builtin.log"
OLD_DATA_FILE = FW / "data" / "otgw_simulation.log"


def read_text(path):
    return path.read_bytes().decode("utf-8").replace("\r\n", "\n")


def function_body(src, name):
    """Text of `<type> name(...) { ... }`, cut out by brace matching."""
    m = re.search(r"^(?:static\s+)?[\w:<>\s&*]+?\b" + re.escape(name) + r"\s*\(", src, re.M)
    if not m:
        raise AssertionError("function %s not found" % name)
    start = src.index("{", m.end())
    depth = 0
    for i in range(start, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():i + 1]
    raise AssertionError("unbalanced braces in %s" % name)


def header_lines():
    src = read_text(HEADER)
    m = re.search(r"OTGW_SIM_FIXTURE\[\]\s*PROGMEM\s*=(.*?);", src, re.S)
    if not m:
        raise AssertionError("OTGW_SIM_FIXTURE not found in the header")
    lits = re.findall(r'"((?:[^"\\\n]|\\.)*)"', m.group(1))
    lines = []
    for lit in lits:
        if not lit.endswith("\\n"):
            raise AssertionError("fixture literal does not end in a newline escape: %r" % lit)
        lines.append(lit[:-2])
    return lines


class FixtureContent(unittest.TestCase):
    def test_header_equals_canonical_list(self):
        canon = CANON.read_bytes().decode("ascii").splitlines()
        self.assertEqual(header_lines(), canon)

    def test_canonical_list_is_sane(self):
        canon = CANON.read_bytes().decode("ascii").splitlines()
        self.assertEqual(len(canon), 1156)
        for n, line in enumerate(canon, 1):
            self.assertRegex(line, r"^[A-Z][0-9A-F]{8}$", "line %d" % n)

    def test_no_literal_is_split_or_blank(self):
        # Every literal is one full line: the concatenation has exactly one
        # newline escape per literal and no empty line.
        lines = header_lines()
        self.assertTrue(all(lines))

    def test_data_file_is_gone_from_the_image_sources(self):
        self.assertFalse(OLD_DATA_FILE.exists(),
                         "%s must not exist; the fixture ships in the firmware" % OLD_DATA_FILE)


class ReplaySourceInCore(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = read_text(CORE)

    def test_missing_file_no_longer_disables_the_replay(self):
        self.assertNotIn("simulation disabled: /otgw_simulation.log not found", self.src)
        self.assertNotIn("[/otgw_simulation.log missing]", self.src)
        self.assertNotIn("replay file reopen failed", self.src)
        self.assertNotIn("OTGW simulation disabled: LittleFS not mounted", self.src)

    def test_core_includes_the_fixture_header(self):
        self.assertIn('#include "otgwSimulationFixture.h"', self.src)

    def test_reopen_prefers_the_file_then_falls_back_to_the_builtin_source(self):
        body = function_body(self.src, "reopenOTGWSimulationFile")
        file_at = body.index("LittleFS.exists")
        self.assertIn("LittleFS.open", body)
        self.assertIn("OTGWReplaySource::File", body)
        builtin_at = body.index("OTGWReplaySource::Builtin")
        self.assertLess(file_at, builtin_at, "the file check must come before the built-in fallback")
        self.assertIn("LittleFSmounted", body)

    def test_builtin_bytes_come_from_progmem(self):
        body = function_body(self.src, "nextOTGWSimulationByte")
        self.assertIn("pgm_read_byte(OTGW_SIM_FIXTURE", body)
        self.assertIn("OTGW_SIM_FIXTURE_LEN", body)
        self.assertIn("return -1", body)

    def test_reader_uses_the_source_neutral_byte_getter(self):
        body = function_body(self.src, "readOTGWSimulationLine")
        self.assertIn("nextOTGWSimulationByte(replayFile)", body)
        self.assertNotIn("replayFile.read()", body)

    def test_wrap_around_reopens_the_source(self):
        body = function_body(self.src, "replayNextOTGWSimulationLine")
        self.assertIn("reopenOTGWSimulationFile(otgwSimulationFile)", body)

    def test_pump_decides_on_the_source_state_not_on_file_truthiness(self):
        body = function_body(self.src, "handlePICSerialSimulation")
        self.assertIn("otgwReplaySource == OTGWReplaySource::None", body)
        self.assertNotIn("!otgwSimulationFile", body,
                         "File stays false for the built-in source; the pump must not key on it")
        self.assertEqual(body.count("closeOTGWSimulationSource(otgwSimulationFile)"), 3,
                         "enable edge, disable edge and the start re-select must each reset the source choice")
        self.assertNotIn("bOTGWSimulation = false", body)

    def test_close_resets_source_and_cursor(self):
        body = function_body(self.src, "closeOTGWSimulationSource")
        self.assertIn("OTGWReplaySource::None", body)
        self.assertIn("otgwReplayBuiltinPos = 0", body)

    def test_ser2net_mirror_is_unchanged(self):
        body = function_body(self.src, "replayNextOTGWSimulationLine")
        self.assertIn("OTGWstream.write(reinterpret_cast<const uint8_t*>(sReplay), replayLen)", body)
        self.assertIn("dispatchOTGWInputLine(sReplay, replayLen)", body)


class SimulateRestApi(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = read_text(RESTAPI)

    def test_status_json_reports_source_and_is_always_available(self):
        body = function_body(self.src, "sendSimulationStatus")
        self.assertIn(r'\"source\":\"%s\"', body)
        self.assertIn('haveFile ? "file" : "builtin"', body)
        self.assertIn(r'\"available\":true', body)
        self.assertNotIn(r'\"available\":false', body)
        self.assertNotIn("reason", body)

    def test_start_has_no_filesystem_refusal(self):
        self.assertNotIn("Frame replay unavailable", self.src)
        self.assertNotIn("otgwSimulationUnavailableReason", self.src)



class ReselectOnStart(unittest.TestCase):
    """Review finding (TASK-1294): a start while the built-in fixture plays must re-pick the
    source, as the status JSON and the docs say. The REST handler runs on the async task, so it
    raises a flag and the loop-side pump re-selects."""

    def test_start_requests_a_reselect(self):
        body = function_body(read_text(RESTAPI), "setOTGWSimulationEnabled")
        self.assertIn("requestOTGWReplayReselect()", body)

    def test_pump_honours_the_reselect_flag(self):
        core = read_text(CORE)
        self.assertIn("static volatile bool g_otgwReplayReselect", core)
        body = function_body(core, "handlePICSerialSimulation")
        i = body.index("g_otgwReplayReselect")
        self.assertIn("closeOTGWSimulationSource", body[i:i + 400])

    def test_lineless_file_falls_back_to_the_builtin_fixture(self):
        body = function_body(read_text(CORE), "replayNextOTGWSimulationLine")
        i = body.index("if (!haveReplayLine)")
        tail = body[i:]
        self.assertIn("OTGWReplaySource::Builtin", tail)
        self.assertIn("iOTGWSimulationNextDueMs", tail)

if __name__ == "__main__":
    unittest.main()
