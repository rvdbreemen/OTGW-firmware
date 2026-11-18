#!/usr/bin/env python3
"""TASK-2016: the capture tool runs on Python 3.9, the oldest version it supports.

otgw_capture.py says "Python 3.9+" and macOS ships 3.9.6 as /usr/bin/python3. On indigo_light's Mac (beta.360
tools) the graph-history CSV failed with "TypeError: write_text() got an unexpected keyword argument 'newline'":
Path.write_text(newline=...) exists only from Python 3.10.

Three checks on the shipped tool (build.py puts scripts/otgw_capture.py in the flash zip):
  1. it parses with the Python 3.9 grammar (ast feature_version), so no match statement and the like;
  2. no call passes a keyword that the 3.9 standard library does not have (write_text/read_text newline=,
     zip strict=);
  3. with a Python 3.9 interpreter available (env OTGW_PY39, or `uv python find 3.9`), the graph-history writer
     tests of tests/test_capture_device_data.py run under 3.9. Skipped, not passed, when there is none.

Old-vs-fix: on the code before TASK-2016, check 2 names otgw_capture.py:2621 and check 3 fails with the TypeError.
Run: python tests/test_capture_py39_compat.py
"""
import ast
import os
import shutil
import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TOOL = REPO / "scripts" / "otgw_capture.py"

# keyword arguments that 3.10+ added to methods the tool may call
NEW_KEYWORDS = {("write_text", "newline"), ("read_text", "newline"), ("zip", "strict")}


def find_py39():
    p = os.environ.get("OTGW_PY39")
    if p and Path(p).exists():
        return p
    uv = shutil.which("uv")
    if uv:
        r = subprocess.run([uv, "python", "find", "3.9"], capture_output=True, text=True)
        cand = r.stdout.strip()
        if r.returncode == 0 and cand and Path(cand).exists():
            return cand
    return None


class Py39Compat(unittest.TestCase):
    def test_parses_with_the_python_39_grammar(self):
        ast.parse(TOOL.read_text(encoding="utf-8"), filename=str(TOOL), feature_version=(3, 9))

    def test_no_call_uses_a_310_keyword(self):
        tree = ast.parse(TOOL.read_text(encoding="utf-8"), filename=str(TOOL))
        hits = []
        for node in ast.walk(tree):
            if not isinstance(node, ast.Call):
                continue
            f = node.func
            name = f.attr if isinstance(f, ast.Attribute) else f.id if isinstance(f, ast.Name) else None
            for kw in node.keywords:
                if (name, kw.arg) in NEW_KEYWORDS:
                    hits.append(f"otgw_capture.py:{node.lineno} {name}({kw.arg}=...)")
        self.assertEqual(hits, [], "Python 3.10+ only keywords; the tool supports 3.9+")

    def test_graph_history_writer_runs_on_python_39(self):
        py = find_py39()
        if not py:
            self.skipTest("no Python 3.9 interpreter (set OTGW_PY39 or install one with `uv python install 3.9`)")
        r = subprocess.run([py, "-m", "unittest", "test_capture_device_data.FetchCase",
                            "test_capture_device_data.DeviceDataThreadCase"],
                           cwd=REPO / "tests", capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr[-3000:])


if __name__ == "__main__":
    unittest.main(verbosity=2)
