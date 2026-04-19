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
import unittest
from pathlib import Path

# Ensure UTF-8 stdout on Windows so check-marks do not crash.
if hasattr(sys.stdout, "buffer"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

# Make evaluate.py importable when running from any CWD.
REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

import evaluate  # noqa: E402


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


if __name__ == "__main__":
    unittest.main(verbosity=2)
