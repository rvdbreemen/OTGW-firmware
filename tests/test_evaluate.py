#!/usr/bin/env python3
"""
Unit tests for evaluate.py helpers. Stdlib unittest so no pip install needed.

Run: python tests/test_evaluate.py
Or:  python -m unittest tests.test_evaluate

These tests pin down the pattern-matching rules that gate code quality. A silent
regression in one of these patterns would remove the guard without anyone noticing,
which is exactly what TASK-297 (TEST-H3) was created to prevent.
"""

import io
import sys
import tempfile
import unittest
from pathlib import Path

# Ensure UTF-8 stdout on Windows so check-marks do not crash.
if hasattr(sys.stdout, "buffer"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

# Make evaluate.py importable when running from any CWD.
REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

import evaluate  # noqa: E402


OTDIRECT_STATUS_HELPER = (
    "static void otDirectBridgeProcessStatus(const char* status) { "
    "otDirectBridgeWriteLine(status, 2); processOT(status, 2); }\n"
)
OTDIRECT_PR_HELPER = (
    "static void otDirectBridgeProcessPRResponse(const char* prLine) { "
    "size_t prLen = strlen(prLine); "
    "otDirectBridgeWriteLine(prLine, prLen); processOT(prLine, prLen); }\n"
)
OTDIRECT_BRIDGE_HELPERS = OTDIRECT_STATUS_HELPER + OTDIRECT_PR_HELPER


class TestIsHotPathFile(unittest.TestCase):
    def test_sat_files_are_hot(self):
        self.assertTrue(evaluate.is_hot_path_file("SATble.ino"))
        self.assertTrue(evaluate.is_hot_path_file("SATcontrol.ino"))
        self.assertTrue(evaluate.is_hot_path_file("SATweather.ino"))

    def test_mqttstuff_is_hot(self):
        self.assertTrue(evaluate.is_hot_path_file("MQTTstuff.ino"))

    def test_restapi_is_hot(self):
        self.assertTrue(evaluate.is_hot_path_file("restAPI.ino"))

    def test_otgw_core_and_otdirect_are_hot(self):
        self.assertTrue(evaluate.is_hot_path_file("OTGW-Core.ino"))
        self.assertTrue(evaluate.is_hot_path_file("OTDirect.ino"))

    def test_non_hot_paths(self):
        self.assertFalse(evaluate.is_hot_path_file("webhook.ino"))
        self.assertFalse(evaluate.is_hot_path_file("helperStuff.ino"))
        self.assertFalse(evaluate.is_hot_path_file("FSexplorer.ino"))


class TestScanStringUsagesDetailed(unittest.TestCase):
    def test_detects_declaration(self):
        content = "void f() {\n  String foo;\n}\n"
        hits = evaluate.scan_string_usages_detailed(content, "X.ino")
        self.assertEqual(hits, ["X.ino:2"])

    def test_detects_assignment(self):
        content = 'void f() {\n  String foo = "bar";\n}\n'
        hits = evaluate.scan_string_usages_detailed(content, "X.ino")
        self.assertEqual(hits, ["X.ino:2"])

    def test_detects_direct_init(self):
        content = "void f() {\n  String foo(someArg);\n}\n"
        hits = evaluate.scan_string_usages_detailed(content, "X.ino")
        self.assertEqual(hits, ["X.ino:2"])

    def test_skips_reference_forms(self):
        # References do not allocate; must NOT be flagged.
        content = (
            "void f() {\n"
            "  const String& body = httpServer.arg(0);\n"
            "  String& other = getSomething();\n"
            "}\n"
        )
        hits = evaluate.scan_string_usages_detailed(content, "X.ino")
        self.assertEqual(hits, [])

    def test_skips_line_comments(self):
        content = "void f() {\n  // String foo = \"bar\";\n}\n"
        hits = evaluate.scan_string_usages_detailed(content, "X.ino")
        self.assertEqual(hits, [])

    def test_skips_block_comments(self):
        content = "/*\n * String foo = \"bar\";\n */\n"
        hits = evaluate.scan_string_usages_detailed(content, "X.ino")
        self.assertEqual(hits, [])

    def test_multiple_hits_collect_line_numbers(self):
        content = (
            'String a;\n'
            '// not this\n'
            'String b = "hi";\n'
            'String c(x);\n'
        )
        hits = evaluate.scan_string_usages_detailed(content, "Y.ino")
        self.assertEqual(hits, ["Y.ino:1", "Y.ino:3", "Y.ino:4"])


class TestScanBinaryCompareCalls(unittest.TestCase):
    def test_detects_strncmp_p(self):
        content = 'void f() {\n  if (strncmp_P(buf, PSTR("x"), 1) == 0) {}\n}\n'
        hits = evaluate.scan_binary_compare_calls(content, "X.ino")
        self.assertEqual(hits, ["X.ino:2"])

    def test_detects_strstr_p(self):
        content = 'void f() {\n  char *p = strstr_P(buf, PSTR("x"));\n}\n'
        hits = evaluate.scan_binary_compare_calls(content, "X.ino")
        self.assertEqual(hits, ["X.ino:2"])

    def test_does_not_detect_memcmp_p(self):
        # memcmp_P is the binary-safe alternative; must NOT be flagged.
        content = 'void f() {\n  if (memcmp_P(buf, PSTR("x"), 1) == 0) {}\n}\n'
        hits = evaluate.scan_binary_compare_calls(content, "X.ino")
        self.assertEqual(hits, [])

    def test_skips_line_comments(self):
        content = "void f() {\n  // strncmp_P(buf, PSTR(\"x\"), 1);\n}\n"
        hits = evaluate.scan_binary_compare_calls(content, "X.ino")
        self.assertEqual(hits, [])

    def test_skips_define_macros(self):
        content = "#define FOO(x) strncmp_P(x, y, 1)\n"
        hits = evaluate.scan_binary_compare_calls(content, "X.ino")
        self.assertEqual(hits, [])


class TestAdrGateHelpers(unittest.TestCase):
    """ADR-080 gate helpers: an Accepted ADR must reference a gate OR carry a
    classification label. These tests pin both paths so a silent regression in
    the markers cannot re-open the drift ADR-080 was written to prevent."""

    def test_accepted_status_detected(self):
        self.assertTrue(evaluate.adr_is_accepted("# ADR-X\n\n## Status\n\nAccepted (2026-04-19)\n"))
        self.assertTrue(evaluate.adr_is_accepted("## Status\n\nAccepted\n"))

    def test_non_accepted_status_ignored(self):
        self.assertFalse(evaluate.adr_is_accepted("## Status\n\nProposed\n"))
        self.assertFalse(evaluate.adr_is_accepted("## Status\n\nSuperseded by ADR-Y\n"))
        self.assertFalse(evaluate.adr_is_accepted(""))

    def test_classification_label_satisfies_gate(self):
        for label in ("guideline-level", "structural", "historical", "policy", "meta-level"):
            text = f"## Status\n\nAccepted ({label})\n"
            self.assertTrue(evaluate.adr_has_gate_or_label(text),
                            f"label '{label}' should satisfy the gate requirement")

    def test_gate_reference_satisfies_gate(self):
        for marker in ("evaluate.py check_progmem_compliance", "tests/test_evaluate.py", "evaluate.py"):
            text = f"## Status\n\nAccepted\n\n## Consequences\n\nEnforced by {marker}\n"
            self.assertTrue(evaluate.adr_has_gate_or_label(text),
                            f"gate marker '{marker}' should satisfy the gate requirement")

    def test_accepted_without_gate_or_label_fails(self):
        text = ("# ADR-Y Some rule\n\n## Status\n\nAccepted\n\n"
                "## Context\n\nStuff happens.\n\n## Decision\n\nDo the thing.\n")
        self.assertTrue(evaluate.adr_is_accepted(text))
        self.assertFalse(evaluate.adr_has_gate_or_label(text))


class TestBacklogHygieneHelpers(unittest.TestCase):
    """AC Deviation Protocol: Done tasks with unchecked ACs must cite a
    deviation / follow-up / scope-out. These tests keep the marker list honest."""

    _DONE_TASK_ALL_CHECKED = (
        "---\nid: TASK-1\nstatus: Done\n---\n\n"
        "## Acceptance Criteria\n"
        "<!-- AC:BEGIN -->\n"
        "- [x] #1 First done\n"
        "- [x] #2 Second done\n"
        "<!-- AC:END -->\n"
    )
    _DONE_TASK_ONE_UNCHECKED_NO_NOTE = (
        "---\nid: TASK-2\nstatus: Done\n---\n\n"
        "## Acceptance Criteria\n"
        "- [x] #1 Done\n"
        "- [ ] #2 Not done\n"
    )
    _DONE_TASK_ONE_UNCHECKED_WITH_NOTE = (
        "---\nid: TASK-3\nstatus: Done\n---\n\n"
        "## Acceptance Criteria\n"
        "- [x] #1 Done\n"
        "- [ ] #2 Not done\n\n"
        "## Implementation Notes\n\n"
        "AC2 deferred to hardware test; tracked as follow-up.\n"
    )
    _IN_PROGRESS_TASK = (
        "---\nid: TASK-4\nstatus: In Progress\n---\n\n"
        "## Acceptance Criteria\n"
        "- [ ] #1 Still open\n"
    )

    def test_status_parsing(self):
        self.assertEqual(evaluate.task_status(self._DONE_TASK_ALL_CHECKED), "Done")
        self.assertEqual(evaluate.task_status(self._IN_PROGRESS_TASK), "In")  # "In Progress" splits on whitespace
        # Full status retrieval is done via front-matter parsers in real code; the
        # helper is used only for equality checks against "Done", which works here.

    def test_unchecked_ac_count(self):
        self.assertEqual(evaluate.task_unchecked_ac_count(self._DONE_TASK_ALL_CHECKED), 0)
        self.assertEqual(evaluate.task_unchecked_ac_count(self._DONE_TASK_ONE_UNCHECKED_NO_NOTE), 1)
        self.assertEqual(evaluate.task_unchecked_ac_count(self._DONE_TASK_ONE_UNCHECKED_WITH_NOTE), 1)

    def test_deviation_note_detected(self):
        self.assertTrue(evaluate.task_has_deviation_note(self._DONE_TASK_ONE_UNCHECKED_WITH_NOTE))

    def test_deviation_note_missing(self):
        self.assertFalse(evaluate.task_has_deviation_note(self._DONE_TASK_ONE_UNCHECKED_NO_NOTE))

    def test_various_deviation_markers_all_recognized(self):
        for marker in (
            "AC2 deferred to hardware test",
            "AC-2 deviation: scope pushed to follow-up",
            "Out of scope for this batch",
            "Won't fix -- deliberate beta-debug feature",
            "Manual test required; no CI gate possible",
            "Superseded by TASK-X",
        ):
            text = f"## Implementation Notes\n\n{marker}\n"
            self.assertTrue(evaluate.task_has_deviation_note(text),
                            f"marker phrase '{marker}' should be recognized as a deviation note")


class TestDesignSystemDriftHelpers(unittest.TestCase):
    def test_extract_classes_from_html_simple(self):
        html = (
            "<div class=\"alpha beta\"></div>\n"
            "<span class='gamma'></span>\n"
            "<p class=\"delta\"></p>\n"
        )
        self.assertEqual(
            evaluate.extract_classes_from_html(html),
            [("alpha", 1), ("beta", 1), ("gamma", 2), ("delta", 3)],
        )

    def test_extract_classes_from_js_classlist_api(self):
        js = (
            "el.classList.add('is-open');\n"
            "el.classList.remove(\"is-closed\");\n"
            "el.classList.toggle('active');\n"
            "el.classList.replace('old-state', 'new-state');\n"
            "el.classList.contains('ready');\n"
            "el.classList.remove('stale-one', 'stale-two', 'stale-three');\n"
        )
        classes = [cls for cls, _ in evaluate.extract_classes_from_js(js)]
        self.assertEqual(
            classes,
            [
                "is-open",
                "is-closed",
                "active",
                "old-state",
                "new-state",
                "ready",
                "stale-one",
                "stale-two",
                "stale-three",
            ],
        )

    def test_extract_classes_from_js_template_literal(self):
        js = "const row = `<div class=\"row is-live\"><span>OK</span></div>`;\n"
        self.assertEqual(
            evaluate.extract_classes_from_js(js),
            [("row", 1), ("is-live", 1)],
        )

    def test_extract_classes_from_js_skips_template_placeholders(self):
        js = "const row = `<div class=\"row ${stateClass} fixed ${prefix}-dynamic\"></div>`;\n"
        classes = [cls for cls, _ in evaluate.extract_classes_from_js(js)]
        self.assertEqual(classes, ["row", "fixed"])

    def test_extract_class_definitions_compound_selectors(self):
        css = ".foo.bar, .foo .bar, .foo:hover, .baz::before { color: red; }\n"
        self.assertEqual(
            evaluate.extract_class_definitions_from_css(css),
            {"foo", "bar", "baz"},
        )

    def test_extract_class_definitions_skips_comments(self):
        css = "/* .ghost { color: red; } */\n.real { color: green; }\n"
        self.assertNotIn("ghost", evaluate.extract_class_definitions_from_css(css))
        self.assertIn("real", evaluate.extract_class_definitions_from_css(css))

    def test_css_and_js_comment_stripping(self):
        self.assertEqual(evaluate.strip_css_comments(".real{} /* .ghost{} */"), ".real{} ")
        cleaned_js = evaluate.strip_js_comments(
            "el.classList.add('real'); // el.classList.add('line-ghost')\n"
            "/* el.classList.add('block-ghost') */\n"
        )
        self.assertIn("real", cleaned_js)
        self.assertNotIn("line-ghost", cleaned_js)
        self.assertNotIn("block-ghost", cleaned_js)

    def test_compute_drift_set_arithmetic_and_allowlist(self):
        used = {
            "used-missing": ["index.html:1"],
            "allowed-hook": ["index.html:2"],
        }
        defined = {"defined-unused"}
        missing, dead, allowlisted = evaluate.compute_drift(
            used,
            defined,
            {"allowed-hook"},
        )
        self.assertEqual(missing, {"used-missing": ["index.html:1"]})
        self.assertEqual(dead, {"defined-unused"})
        self.assertEqual(allowlisted, {"allowed-hook": ["index.html:2"]})

    def test_drift_against_known_fixture(self):
        with tempfile.TemporaryDirectory() as tmp:
            data_dir = Path(tmp)
            (data_dir / "index.html").write_text(
                '<div class="present missing"></div>\n',
                encoding="utf-8",
            )
            (data_dir / "app.js").write_text(
                "el.classList.add('allowed-hook');\n",
                encoding="utf-8",
            )
            (data_dir / "style.css").write_text(
                ".present { display: block; }\n",
                encoding="utf-8",
            )

            used, defined, _ = evaluate.scan_design_system_workspace(data_dir)
            missing, _, allowlisted = evaluate.compute_drift(
                used,
                defined,
                {"allowed-hook"},
            )

        self.assertEqual(missing, {"missing": ["index.html:1"]})
        self.assertEqual(allowlisted, {"allowed-hook": ["app.js:1"]})


class TestOTDirect25238BridgeRegression(unittest.TestCase):
    """Pin the ESP32 25238 bridge split so a future refactor cannot silently
    remove the cooperative socket service or move the no-PIC bridge out of
    OTDirect again."""

    def test_current_source_wiring_is_detected(self):
        handle_debug = (REPO_ROOT / "src" / "OTGW-firmware" / "handleDebug.ino").read_text(encoding="utf-8", errors="ignore")
        firmware = (REPO_ROOT / "src" / "OTGW-firmware" / "OTGW-firmware.ino").read_text(encoding="utf-8", errors="ignore")
        otdirect = (REPO_ROOT / "src" / "OTGW-firmware" / "OTDirect.ino").read_text(encoding="utf-8", errors="ignore")

        checks = evaluate.otdirect_25238_bridge_regressions(handle_debug, firmware, otdirect)
        self.assertEqual(
            checks,
            {
                "service_loop": True,
                "inbound_bridge": True,
                "outbound_fanout": True,
                "short_error_fanout": True,
                "pr_response_fanout": True,
                "ownership_split": True,
            },
        )

    def test_missing_service_loop_is_flagged(self):
        handle_debug = "void handleDebug() {}\n"
        firmware = "void doBackgroundTasks() { handleOTDirectBridgeStream(); }\n"
        otdirect = OTDIRECT_BRIDGE_HELPERS + (
            "void handleOTDirectBridgeStream() { OTGWstream.available(); OTGWstream.read(); sendPICSerial(\"x\", 1); }\n"
            "static void bridgeFrameToParser(char prefix, unsigned long frame) { otDirectBridgeWriteLine(buf, 9); }\n"
            "static void synthesizeResponse(char c0, char c1, const char* value) { otDirectBridgeWriteLine(buf, respLen); }\n"
            "OTGWstream.write((const uint8_t*)\"x\", 1);\n"
        )

        checks = evaluate.otdirect_25238_bridge_regressions(handle_debug, firmware, otdirect)
        self.assertFalse(checks["service_loop"])
        self.assertTrue(checks["inbound_bridge"])
        self.assertTrue(checks["outbound_fanout"])
        self.assertTrue(checks["short_error_fanout"])
        self.assertTrue(checks["pr_response_fanout"])
        self.assertTrue(checks["ownership_split"])

    def test_unserviced_helper_is_flagged(self):
        handle_debug = "void handleOTGWstream() { OTGWstream.loop(); }\n"
        firmware = "void doBackgroundTasks() { handleOTDirectBridgeStream(); }\n"
        otdirect = OTDIRECT_BRIDGE_HELPERS + (
            "void handleOTDirectBridgeStream() { OTGWstream.available(); OTGWstream.read(); sendPICSerial(\"x\", 1); }\n"
            "static void bridgeFrameToParser(char prefix, unsigned long frame) { otDirectBridgeWriteLine(buf, 9); }\n"
            "static void synthesizeResponse(char c0, char c1, const char* value) { otDirectBridgeWriteLine(buf, respLen); }\n"
            "OTGWstream.write((const uint8_t*)\"x\", 1);\n"
        )

        checks = evaluate.otdirect_25238_bridge_regressions(handle_debug, firmware, otdirect)
        self.assertFalse(checks["service_loop"])
        self.assertTrue(checks["inbound_bridge"])
        self.assertTrue(checks["outbound_fanout"])
        self.assertTrue(checks["short_error_fanout"])
        self.assertTrue(checks["pr_response_fanout"])
        self.assertTrue(checks["ownership_split"])

    def test_missing_inbound_bridge_is_flagged(self):
        handle_debug = "void handleOTGWstream() { OTGWstream.loop(); }\n"
        firmware = "void doBackgroundTasks() { handleOTGWstream(); handleOTDirectBridgeStream(); }\n"
        otdirect = OTDIRECT_BRIDGE_HELPERS + (
            "void handleOTDirectBridgeStream() {}\n"
            "static void bridgeFrameToParser(char prefix, unsigned long frame) { otDirectBridgeWriteLine(buf, 9); }\n"
            "static void synthesizeResponse(char c0, char c1, const char* value) { otDirectBridgeWriteLine(buf, respLen); }\n"
            "OTGWstream.write((const uint8_t*)\"x\", 1);\n"
        )

        checks = evaluate.otdirect_25238_bridge_regressions(handle_debug, firmware, otdirect)
        self.assertTrue(checks["service_loop"])
        self.assertFalse(checks["inbound_bridge"])
        self.assertTrue(checks["outbound_fanout"])
        self.assertTrue(checks["short_error_fanout"])
        self.assertTrue(checks["pr_response_fanout"])
        self.assertTrue(checks["ownership_split"])

    def test_missing_outbound_fanout_is_flagged(self):
        handle_debug = "void handleOTGWstream() { OTGWstream.loop(); }\n"
        firmware = "void doBackgroundTasks() { handleOTGWstream(); handleOTDirectBridgeStream(); }\n"
        otdirect = OTDIRECT_BRIDGE_HELPERS + (
            "void handleOTDirectBridgeStream() { OTGWstream.available(); OTGWstream.read(); sendPICSerial(\"x\", 1); }\n"
            "static void bridgeFrameToParser(char prefix, unsigned long frame) { processOT(buf, 9, otHideReports); }\n"
            "static void synthesizeResponse(char c0, char c1, const char* value) { processOT(buf, respLen); }\n"
        )

        checks = evaluate.otdirect_25238_bridge_regressions(handle_debug, firmware, otdirect)
        self.assertTrue(checks["service_loop"])
        self.assertTrue(checks["inbound_bridge"])
        self.assertFalse(checks["outbound_fanout"])
        self.assertTrue(checks["short_error_fanout"])
        self.assertTrue(checks["pr_response_fanout"])
        self.assertTrue(checks["ownership_split"])

    def test_missing_short_error_fanout_is_flagged(self):
        handle_debug = "void handleOTGWstream() { OTGWstream.loop(); }\n"
        firmware = "void doBackgroundTasks() { handleOTGWstream(); handleOTDirectBridgeStream(); }\n"
        otdirect = OTDIRECT_PR_HELPER + (
            "void handleOTDirectBridgeStream() { OTGWstream.available(); OTGWstream.read(); sendPICSerial(\"x\", 1); }\n"
            "static void bridgeFrameToParser(char prefix, unsigned long frame) { otDirectBridgeWriteLine(buf, 9); }\n"
            "static void synthesizeResponse(char c0, char c1, const char* value) { otDirectBridgeWriteLine(buf, respLen); }\n"
            "OTGWstream.write((const uint8_t*)\"x\", 1);\n"
            "void handleOTDirectCommand(const char* buf, int len) { processOT(\"NG\", 2); }\n"
        )

        checks = evaluate.otdirect_25238_bridge_regressions(handle_debug, firmware, otdirect)
        self.assertTrue(checks["service_loop"])
        self.assertTrue(checks["inbound_bridge"])
        self.assertTrue(checks["outbound_fanout"])
        self.assertFalse(checks["short_error_fanout"])
        self.assertTrue(checks["pr_response_fanout"])
        self.assertTrue(checks["ownership_split"])

    def test_missing_pr_response_fanout_is_flagged(self):
        handle_debug = "void handleOTGWstream() { OTGWstream.loop(); }\n"
        firmware = "void doBackgroundTasks() { handleOTGWstream(); handleOTDirectBridgeStream(); }\n"
        otdirect = OTDIRECT_STATUS_HELPER + (
            "void handleOTDirectBridgeStream() { OTGWstream.available(); OTGWstream.read(); sendPICSerial(\"x\", 1); }\n"
            "static void bridgeFrameToParser(char prefix, unsigned long frame) { otDirectBridgeWriteLine(buf, 9); }\n"
            "static void synthesizeResponse(char c0, char c1, const char* value) { otDirectBridgeWriteLine(buf, respLen); }\n"
            "OTGWstream.write((const uint8_t*)\"x\", 1);\n"
            "void handleOTDirectCommand(const char* buf, int len) { processOT(prBuf, strlen(prBuf)); }\n"
        )

        checks = evaluate.otdirect_25238_bridge_regressions(handle_debug, firmware, otdirect)
        self.assertTrue(checks["service_loop"])
        self.assertTrue(checks["inbound_bridge"])
        self.assertTrue(checks["outbound_fanout"])
        self.assertTrue(checks["short_error_fanout"])
        self.assertFalse(checks["pr_response_fanout"])
        self.assertTrue(checks["ownership_split"])

    def test_missing_ownership_split_is_flagged(self):
        handle_debug = "void handleOTGWstream() { OTGWstream.loop(); }\n"
        firmware = "void doBackgroundTasks() { handleOTGWstream(); handleOTDirectBridgeStream(); }\n"
        otdirect = OTDIRECT_BRIDGE_HELPERS + (
            "void handleOTDirectBridgeStream() { OTGWstream.available(); OTGWstream.read(); sendPICSerial(\"x\", 1); handlePICSerial(); }\n"
            "static void bridgeFrameToParser(char prefix, unsigned long frame) { otDirectBridgeWriteLine(buf, 9); }\n"
            "static void synthesizeResponse(char c0, char c1, const char* value) { otDirectBridgeWriteLine(buf, respLen); }\n"
            "OTGWstream.write((const uint8_t*)\"x\", 1);\n"
        )

        checks = evaluate.otdirect_25238_bridge_regressions(handle_debug, firmware, otdirect)
        self.assertTrue(checks["service_loop"])
        self.assertTrue(checks["inbound_bridge"])
        self.assertTrue(checks["outbound_fanout"])
        self.assertTrue(checks["short_error_fanout"])
        self.assertTrue(checks["pr_response_fanout"])
        self.assertFalse(checks["ownership_split"])


class TestProgmemComplianceIntegration(unittest.TestCase):
    """Smoke test: confirm the main evaluator wiring still works end-to-end."""

    def test_evaluator_runs_quick_without_crashing(self):
        # Run the full --quick pipeline against the real project. We do not
        # assert on counts (those change over time); we only confirm the run
        # completes and produces some results.
        evaluator = evaluate.WorkspaceEvaluator(REPO_ROOT, verbose=False)
        evaluator.evaluate_all(quick=True)
        self.assertGreater(len(evaluator.results), 0)
        categories = {r.category for r in evaluator.results}
        self.assertIn("PROGMEM", categories)
        self.assertIn("Coding", categories)  # binary-safe compare audit
        self.assertIn("Design System", categories)
        bridge_checks = [r for r in evaluator.results if r.name == "OTDirect 25238 bridge audit"]
        self.assertEqual(len(bridge_checks), 1)
        self.assertEqual(bridge_checks[0].status, "PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
