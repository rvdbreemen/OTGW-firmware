#!/usr/bin/env python3
"""TASK-1899 gate: no LittleFS call in a REST handler that runs on the async_tcp task.

Every function in restAPI.ino runs from the ESPAsyncWebServer callbacks, so on the async_tcp task. A LittleFS call
there is a cross-core flash read (or write): when it hangs, the loop task on the same core starves into the task
watchdog (TASK-1865 health crash, beta.182; TASK-1899 /api/v2/simulate crash, gruijter 13 Nov 04:00:54). File work
belongs on the loop task; REST reads what the loop task prepared.

The scanner blanks comments and string literals, maps every `LittleFS.<call>` to the top-level function around it, and
compares the count per function with ALLOWED. A function that is not listed, or a listed one with more calls than its
entry, fails the gate. The entries are known handlers that still do file work on async_tcp; each names the follow-up
task that moves it to the loop task and removes the entry.

OTGW_RESTAPI_INO=<copy of an older restAPI.ino> runs the gate against other code (red on the code before TASK-1899).

Run: python tests/test_rest_no_littlefs_on_async_tcp.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
REST_INO = Path(os.environ.get("OTGW_RESTAPI_INO", REPO / "src" / "OTGW-firmware" / "restAPI.ino"))

# function -> (LittleFS calls allowed, follow-up task that removes the entry)
ALLOWED = {
    "handleSAT": (9, "TASK-1911"),               # heating-curve calibration markers file (TASK-586): read and write
    "getDallasLabels": (1, "TASK-1912"),         # /dallas_labels.ini read
    "updateAllDallasLabels": (1, "TASK-1912"),   # /dallas_labels.ini write
}


def blank_comments_and_strings(code: str) -> str:
    """Replace comments, string and char literals by spaces; newlines and offsets stay."""
    out, i, n = [], 0, len(code)
    while i < n:
        c = code[i]
        if code.startswith("//", i):
            j = code.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i)); i = j
        elif code.startswith("/*", i):
            j = code.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", code[i:j])); i = j
        elif c in "\"'":
            j = i + 1
            while j < n and code[j] != c:
                j += 2 if code[j] == "\\" else 1
            j += 1
            out.append(c + " " * max(0, j - i - 2) + c); i = j
        else:
            out.append(c); i += 1
    return "".join(out)


_HEAD = re.compile(r"([A-Za-z_]\w*)\s*\([^;{}]*\)\s*(?:const\s*)?$")


def littlefs_calls(src: str):
    """[(function, line, call)] for every LittleFS.<name> outside comments and strings."""
    s = blank_comments_and_strings(src.replace("\r\n", "\n"))
    depth, cur, found = 0, None, []
    for m in re.finditer(r"[{}]|\bLittleFS\.\w+", s):
        t = m.group(0)
        if t == "{":
            if depth == 0:
                h = _HEAD.search(s[max(0, m.start() - 300):m.start()].rstrip())
                cur = h.group(1) if h else "?"
            depth += 1
        elif t == "}":
            depth -= 1
            if depth == 0:
                cur = None
        else:
            found.append((cur or "<file scope>", s.count("\n", 0, m.start()) + 1, t))
    return found


def violations(src: str):
    per = {}
    for fn, line, call in littlefs_calls(src):
        per.setdefault(fn, []).append((line, call))
    bad = []
    for fn, calls in sorted(per.items()):
        allowed = ALLOWED.get(fn, (0, None))[0]
        if len(calls) > allowed:
            where = ", ".join("%d %s" % c for c in calls)
            bad.append("%s: %d LittleFS call(s), %d allowed (%s)" % (fn, len(calls), allowed, where))
    return bad, per


def function_body(src: str, name: str) -> str:
    s = src.replace("\r\n", "\n")
    m = re.search(r"\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", s)
    if not m:
        raise AssertionError(name + " not found in " + str(REST_INO))
    blank = blank_comments_and_strings(s)
    i, depth = m.end() - 1, 0
    for j in range(i, len(blank)):
        if blank[j] == "{":
            depth += 1
        elif blank[j] == "}":
            depth -= 1
            if depth == 0:
                return s[i:j + 1]
    raise AssertionError("unbalanced braces in " + name)


class RestLittleFSGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = REST_INO.read_text(encoding="utf-8")

    def test_no_unlisted_littlefs_call_in_a_rest_handler(self):
        bad, _ = violations(self.src)
        self.assertEqual(bad, [], "LittleFS on the async_tcp task (move it to the loop task):\n  " + "\n  ".join(bad))

    def test_allow_list_is_not_stale(self):
        # an entry whose handler no longer touches LittleFS must go, so the list only names real exceptions
        _, per = violations(self.src)
        stale = [fn for fn in ALLOWED if fn not in per]
        self.assertEqual(stale, [], "remove these ALLOWED entries, their handlers no longer call LittleFS")

    def test_gate_fails_on_an_unlisted_call(self):
        # the gate itself: one LittleFS call added to a handler that is not listed must be reported
        body = function_body(self.src, "sendSimulationStatus")
        mutated = self.src.replace(body, body[:-1] + "  (void)LittleFS.exists(\"/x\");\n}", 1)
        bad, _ = violations(mutated)
        self.assertTrue(any(b.startswith("sendSimulationStatus:") for b in bad), bad)

    def test_gate_ignores_comments_and_strings(self):
        bad, _ = violations('void f() {\n  // LittleFS.exists("/a")\n  const char* t = "LittleFS.open";\n}\n')
        self.assertEqual(bad, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
