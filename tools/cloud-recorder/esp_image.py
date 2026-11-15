"""Reads strings from an ESP32 application image (firmware.bin) by their load address (TASK-1938).

The image is a 24-byte header (magic 0xE9, segment count at byte 1), then per segment a load address (u32),
a length (u32) and the data. The flash-mapped read-only data (DROM, 0x3C000000 on the ESP32-S3) holds the
format strings and function names that the debug macros pass, at the address the firmware uses at run time.
"""
import struct

DROM_BASE = 0x3C000000


class EspImage:
    def __init__(self, path):
        data = open(path, "rb").read()
        if data[0] != 0xE9:
            raise ValueError(f"{path}: not an ESP application image (magic {data[0]:#x})")
        count, pos = data[1], 24
        self.segments = []
        for _ in range(count):
            addr, length = struct.unpack_from("<II", data, pos)
            self.segments.append((addr, data[pos + 8:pos + 8 + length]))
            pos += 8 + length

    def drom(self):
        return [(a, d) for a, d in self.segments if 0x3C000000 <= a < 0x3E000000]

    def read_cstr(self, addr, limit=512):
        for base, d in self.segments:
            if base <= addr < base + len(d):
                off = addr - base
                end = d.find(b"\x00", off, off + limit)
                return d[off:end if end >= 0 else off + limit].decode("utf-8", errors="replace")
        raise KeyError(f"address {addr:#x} not in the image")

    def find_cstr(self, text):
        """Address of a whole C string with this text in DROM, or None."""
        needle = b"\x00" + text.encode("utf-8") + b"\x00"
        for base, d in self.drom():
            i = d.find(needle)
            if i >= 0:
                return base + i + 1
        return None
