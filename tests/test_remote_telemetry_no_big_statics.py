#!/usr/bin/env python3
"""TASK-2901 poc.19: remote telemetry holds no large static buffers in internal DRAM.

poc.18 declared its open record (rtel::Recorder, ~13.5 KB), send buffer (REC_MAX_BYTES, ~4 KB), LZ work area
(rtel::LzWork, 3.2 KB) and login reply (char[1536]) as file-scope statics: ~24 KB of internal DRAM on every boot, also
with the feature off. On .63 (Classic + S3, BLE on) that left ~8 KB free; the unit hung on an upload and then looped on
the interrupt watchdog (09, 2026-11-19 19:04). poc.19 allocates them in remoteTelemetryInitCore() (PSRAM first).

This scans the remote telemetry headers for a file-scope `static` object of one of those types, or a static array of
1024 bytes or more. Red on e4fb77fb2 (poc.18): 4 hits. Green on the fix.
Run: python tests/test_remote_telemetry_no_big_statics.py
"""
import re
import unittest
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src" / "OTGW-firmware"
FILES = sorted(SRC.glob("RemoteTelemetry*.h")) + sorted(SRC.glob("RemoteTelemetry*.ino"))
BIG_TYPES = re.compile(r"^static\s+(?:const\s+)?rtel::(Recorder|LzWork)\s+\w+\s*;")
BIG_ARRAY = re.compile(r"^static\s+(?:volatile\s+)?(?:u?int8_t|char|uint16_t|uint32_t)\s+(\w+)\s*\[\s*([^\]]+)\]")


def array_bytes(dim, elem):
    size = {"char": 1, "int8_t": 1, "uint8_t": 1, "uint16_t": 2, "uint32_t": 4}[elem]
    if "REC_MAX_BYTES" in dim:
        return 4096 * size            # REC_MAX_BYTES is ~4 KB (RemoteTelemetryCodec.h)
    try:
        return int(dim, 0) * size
    except ValueError:
        return 0


def hits():
    out = []
    for f in FILES:
        for n, line in enumerate(f.read_text(encoding="utf-8").replace("\r\n", "\n").splitlines(), 1):
            if BIG_TYPES.match(line):
                out.append(f"{f.name}:{n}: {line.strip()}")
                continue
            m = BIG_ARRAY.match(line)
            if m:
                elem = line.split()[1] if line.split()[1] != "volatile" else line.split()[2]
                if array_bytes(m.group(2), elem) >= 1024:
                    out.append(f"{f.name}:{n}: {line.strip()}")
    return out


class NoBigStatics(unittest.TestCase):
    def test_files_found(self):
        self.assertTrue(any(f.name == "RemoteTelemetryDevice.h" for f in FILES), FILES)

    def test_no_large_static_buffer_in_internal_dram(self):
        h = hits()
        self.assertEqual(h, [], "large file-scope statics (internal DRAM on every boot):\n" + "\n".join(h))

    def test_buffers_are_allocated_in_init_core(self):
        s = (SRC / "RemoteTelemetryDevice.h").read_text(encoding="utf-8").replace(chr(13) + chr(10), chr(10))
        a = s.index("static bool remoteTelemetryInitCore()")
        body = s[a:s.index("\n}\n", a)]
        self.assertIn("platformPsramAllocOnce(sizeof(RemoteTelemetryBufs))", body)
        self.assertIn("remoteTelemetrySlots[0].buf = remoteTelemetrySendBuf", body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
