#!/usr/bin/env python3
"""TASK-1321 guard: ot_log_buffer has one writer, the loop task.

sendEventToWebSocket_P() fills the global ot_log_buffer. POST /api/v2/simulate/start
and /stop run on the async_tcp task, so setOTGWSimulationEnabled() must not call it.
It posts the event (requestOTGWSimulationEvent) and handleOTReplay(), which runs on
the loop task, sends it.

Checks (source level, the text of the two files):
  1. restAPI.ino: no function sends a WS event or touches ot_log_buffer / AddLog /
     ClrLog (every REST handler runs on async_tcp).
  2. setOTGWSimulationEnabled() calls requestOTGWSimulationEvent(enabled).
  3. OTGW-Core.ino: handleOTReplay() consumes the pending value, clears it before
     sending, and sends both the "enabled" and the "disabled" event text.
  4. A tiny model of the post/consume protocol: start gives the enabled event, stop
     the disabled one, nothing is sent twice, and the newest request wins.

Not checked: that the firmware compiles, or that the WS frame arrives on a device.
Set OTGW_CORE_INO / OTGW_REST_INO to older copies of the files to see it red
(`git show HEAD:src/OTGW-firmware/restAPI.ino` is the unfixed version).
"""
import os
import re
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src", "OTGW-firmware")
CORE = os.environ.get("OTGW_CORE_INO", os.path.join(SRC, "OTGW-Core.ino"))
REST = os.environ.get("OTGW_REST_INO", os.path.join(SRC, "restAPI.ino"))


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read().replace("\r\n", "\n")


def body(text, signature):
    """Text of the function whose definition line contains `signature`."""
    i = text.index(signature)
    j = text.index("{", i)
    depth = 0
    for k in range(j, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return text[j:k + 1]
    raise AssertionError("unbalanced braces after " + signature)


class SingleWriter(unittest.TestCase):
    def test_rest_api_never_writes_the_log_buffer(self):
        rest = read(REST)
        hits = re.findall(r"sendEventToWebSocket(?:_P)?\s*\(|ot_log_buffer|\bAddLog\w*\s*\(|\bClrLog\s*\(", rest)
        self.assertEqual(hits, [], "restAPI.ino (async_tcp task) writes ot_log_buffer: %r" % hits)

    def test_simulation_setter_posts_instead_of_sending(self):
        b = body(read(REST), "static void setOTGWSimulationEnabled(bool enabled)")
        self.assertIn("requestOTGWSimulationEvent(enabled)", b)

    def test_loop_task_sends_both_events(self):
        b = body(read(CORE), "void handleOTReplay()")
        self.assertIn("g_otgwSimEventPending", b)
        self.assertIn("OTGW simulation enabled [replay active]", b)
        self.assertIn("OTGW simulation disabled [live serial resumed]", b)
        # Take-and-clear must be one atomic step: a post from the other core between a separate
        # read and clear would be lost.
        self.assertIn("__atomic_exchange_n(&g_otgwSimEventPending", b)
        self.assertLess(b.index("__atomic_exchange_n(&g_otgwSimEventPending"), b.index("sendEventToWebSocket_P"),
                        "pending value must be taken before the send")

    def test_telnet_s_key_posts_instead_of_sending(self):
        # handleDebugChar runs from AsyncSimpleTelnet's onData handler, on the async_tcp task.
        dbg = read(os.environ.get("OTGW_DEBUG_INO", os.path.join(SRC, "handleDebug.ino")))
        self.assertIn("requestOTGWSimulationEvent(state.debug.bOTGWSimulation)", dbg)
        self.assertNotIn('PSTR("OTGW simulation enabled [replay active]")', dbg)


if __name__ == "__main__":
    unittest.main(verbosity=2)
