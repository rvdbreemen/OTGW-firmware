#!/usr/bin/env python3
"""Runner for tests/test_graph_history.cpp (TASK-1296; 7-day ring and served window: TASK-1963, ADR-245).

The C++ test includes the REAL src/OTGW-firmware/graphHistoryStuff.h and jsonEmit.h against the stubs in tests/stubs.
compile_cpp() takes no include paths, so this runner copies the test, the two real headers and the stubs into one
temporary directory and compiles there (g++, clang++ or MSVC). OTGW_GRAPH_REV=<sha> takes the two headers from another
revision (old vs fix); the test file itself always comes from the working tree.

Run: python tests/test_graph_history.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
REV = os.environ.get("OTGW_GRAPH_REV")
HEADERS = ("graphHistoryStuff.h", "jsonEmit.h")


def header_text(name):
    if REV is None:
        return (REPO / "src" / "OTGW-firmware" / name).read_text(encoding="utf-8")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:src/OTGW-firmware/{name}"], capture_output=True,
                          text=True, encoding="utf-8", check=True).stdout


class GraphHistory(unittest.TestCase):
    def test_graph_history_cpp(self):
        with tempfile.TemporaryDirectory() as tmp:
            t = Path(tmp)
            for f in (HERE / "stubs").iterdir():
                if f.is_file():
                    shutil.copy(f, t / f.name)
            for name in HEADERS:
                # <Arduino.h>/<platform.h> as "...": MSVC does not search the source directory for <...>.
                text = re.sub(r"#include\s*<(Arduino|platform)\.h>", r'#include "\1.h"', header_text(name))
                (t / name).write_text(text, encoding="utf-8")
            cpp = t / "test_graph_history.cpp"
            shutil.copy(HERE / "test_graph_history.cpp", cpp)
            exe = compile_cpp(cpp, t / "test_graph_history.exe")
            if exe is None:
                self.skipTest("no C++ compiler (g++, clang++ or MSVC)")
            r = subprocess.run([str(exe)], capture_output=True, text=True)
            print(r.stdout[-4000:])
            self.assertEqual(r.returncode, 0, "test_graph_history.cpp failed:\n" + r.stdout[-4000:] + r.stderr[-1000:])


if __name__ == "__main__":
    unittest.main()
