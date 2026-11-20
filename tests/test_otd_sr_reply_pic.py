#!/usr/bin/env python3
"""TASK-1804 A6: the SR= Data-ID set in OTDirect.ino equals the PIC's.

On the PIC (other-projects/otgw-6.6/gateway.asm), SR= changes the answer to the thermostat of a Data-ID whose
TreatMessage entry (messagetable) is WordResponse, MandatoryWord or ByteResponse, or whose own handler jumps
(goto/bra) to WordResponse for its answers. A handler that only *calls* WordResponse on some paths (MessageID100) is
a recorded difference and stays out. OTDirect.ino holds the set as the bitmap otdPicSrIds[4]; this test recomputes it
from gateway.asm by following each handler's body, compares it word by word, and checks that ByteResponse is exactly
{70, 101} (the IDs that keep the request's high byte in otdSrReply()).

Run: python tests/test_otd_sr_reply_pic.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ASM = REPO / "other-projects" / "otgw-6.6" / "gateway.asm"
INO = Path(os.environ.get("OTGW_OTDIRECT_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
SR_HANDLERS = ("WordResponse", "MandatoryWord", "ByteResponse")
PARTIAL = {"MessageID100"}   # calls WordResponse only with an override request or a bad answer (gateway.asm :3272-3283)


def asm_lines():
    return ASM.read_text(encoding="latin-1").split("\n")


def pic_table(lines):
    start = next(i for i, l in enumerate(lines) if l.startswith("messagetable"))
    table = {}
    for l in lines[start:start + 140]:
        m = re.search(r"goto\s+(\w+)\s*;\s*Data ID (\d+)", l)
        if m:
            table[int(m.group(2))] = m.group(1)
    return table


def handler_jumps(lines, table):
    """Per own handler: (jumps to WordResponse with goto/bra, calls it) over its body up to the next table handler."""
    handlers = set(table.values())
    labels = {}
    for i, l in enumerate(lines):
        m = re.match(r"^([A-Za-z_]\w*)\b", l)
        if m:
            labels.setdefault(m.group(1), i)
    starts = sorted(i for n, i in labels.items() if n in handlers)
    out = {}
    for h in handlers - set(SR_HANDLERS):
        s = labels[h]
        end = min([i for i in starts if i > s] + [len(lines)])
        body = "\n".join(lines[s:end])
        out[h] = (bool(re.search(r"\b(goto|bra)\s+(WordResponse|ByteResponse)\b", body)),
                  bool(re.search(r"\bcall\s+(WordResponse|ByteResponse)\b", body)))
    return out


def pic_sr_ids(lines, table):
    jumps = handler_jumps(lines, table)
    return sorted(i for i, h in table.items()
                  if h in SR_HANDLERS or (h not in PARTIAL and jumps.get(h, (False, False))[0]))


@unittest.skipUnless(ASM.exists(), "other-projects/otgw-6.6/gateway.asm not checked out")
class SrIdSet(unittest.TestCase):
    def setUp(self):
        self.lines = asm_lines()
        self.table = pic_table(self.lines)
        self.src = INO.read_text(encoding="utf-8", errors="replace")

    def test_table_has_128_entries(self):
        self.assertEqual(sorted(self.table), list(range(128)))

    def test_handlers_that_reach_word_response(self):
        jumps = handler_jumps(self.lines, self.table)
        self.assertEqual(sorted(h for h, (j, _) in jumps.items() if j),
                         ["MessageID48", "MessageID49", "MessageID5", "MessageID8", "TSPBufferSize"])
        self.assertEqual(sorted(h for h, (j, c) in jumps.items() if c and not j), sorted(PARTIAL))

    def test_bitmap_equals_the_pic_set(self):
        m = re.search(r"otdPicSrIds\[4\][^{]*\{([^}]*)\}", self.src)
        self.assertIsNotNone(m, "otdPicSrIds[4] not found in OTDirect.ino (no TASK-1804 A6 on this code)")
        words = [int(w, 16) for w in re.findall(r"0x([0-9A-Fa-f]{8})UL", m.group(1))]
        self.assertEqual(len(words), 4)
        want = [0, 0, 0, 0]
        for i in pic_sr_ids(self.lines, self.table):
            want[i >> 5] |= 1 << (i & 31)
        for k in range(4):
            self.assertEqual(words[k], want[k], f"word {k} (IDs {k * 32}-{k * 32 + 31}): 0x{words[k]:08X} != 0x{want[k]:08X}")

    def test_byte_response_ids(self):
        self.assertEqual(sorted(i for i, h in self.table.items() if h == "ByteResponse"), [70, 101])
        self.assertRegex(self.src, r"id\s*==\s*70\s*\|\|\s*id\s*==\s*101")


if __name__ == "__main__":
    unittest.main(verbosity=2)
