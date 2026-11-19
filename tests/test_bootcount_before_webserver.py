"""TASK-1773: the reboot counter is known before the REST API can answer.

The first GET /api/v2/device/info after a reboot answered bootcount 0 (09 on .61, beta.200: 15.1 s after
POST /ReBoot bootcount 0, 15.8 s 433; beta.217: seen again). Cause: setup() opened the web server
(startWebserver(), the only place the port-80 listener starts; handleWebserverListener() retries it from
loop()) before state.uptime.iRebootCount = updateRebootCount(). TASK-1909 (b14cce850) moved the count and
the reboot log before setupFSexplorer() / startWebserver() for its own reason; this test keeps that order.

It reads the REAL setup() of OTGW-firmware.ino and checks, in source order, that the reboot count is
assigned before setupFSexplorer() and startWebserver() are called, and that the listener is only started
from startWebserver() and handleWebserverListener() (FSexplorer.ino). Red on b14cce850^ (the order of the
beta.200 bench), green since.
Run: python tests/test_bootcount_before_webserver.py   (OTGW_SRC_DIR=<dir> for other sources)
"""
import os
import re
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = Path(os.environ.get("OTGW_SRC_DIR", HERE.parent / "src" / "OTGW-firmware"))


def function_body(text, signature):
    m = re.search(r"^" + re.escape(signature) + r"\s*\{", text, re.M)
    assert m, f"{signature} not found"
    i, depth = m.end(), 1
    while depth:
        c = text[i]
        if c == "/" and text[i + 1] == "/":
            i = text.index("\n", i)
            continue
        depth += {"{": 1, "}": -1}.get(c, 0)
        i += 1
    return text[m.end():i]


def code_lines(body):
    return [re.sub(r"//.*", "", l) for l in body.splitlines()]


class BootcountBeforeWebserver(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.setup = code_lines(function_body((SRC / "OTGW-firmware.ino").read_text(encoding="utf-8"), "void setup()"))

    def index_of(self, pattern):
        rx = re.compile(pattern)
        hits = [i for i, l in enumerate(self.setup) if rx.search(l)]
        self.assertTrue(hits, f"{pattern} not found in setup()")
        return hits[0]

    def test_count_assigned_before_the_web_server_starts(self):
        count = self.index_of(r"state\.uptime\.iRebootCount\s*=\s*updateRebootCount\(\)")
        self.assertLess(count, self.index_of(r"\bsetupFSexplorer\(\)\s*;"))
        self.assertLess(count, self.index_of(r"\bstartWebserver\(\)\s*;"))

    def test_listener_only_starts_in_the_two_known_places(self):
        fs = (SRC / "FSexplorer.ino").read_text(encoding="utf-8")
        owners = []
        for m in re.finditer(r"^\s*server\.begin\(\)\s*;", fs, re.M):
            head = fs[:m.start()]
            fn = re.findall(r"^void (\w+)\s*\(", head, re.M)
            owners.append(fn[-1] if fn else "?")
        self.assertEqual(sorted(owners), ["handleWebserverListener", "startWebserver"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
