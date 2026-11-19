#!/usr/bin/env python3
"""Which lossless compression fits the gateway? (TASK-2904)

    python3 compress_study.py --image firmware.bin [--minutes 30]

Builds the same records as proof_all_streams.py (OT frames, SATTICK lines, debug lines from the firmware's own call
sites, one mix record per 60 s, sent early at the 4 KB no-PSRAM buffer) at two debug rates, and compresses every
record on its own (a record must decode alone). Candidates, each lossless:

  raw      the record as the firmware sends it today
  zlib9    deflate level 9, reference only: its compressor state (~300 KB) does not fit a board without PSRAM
  lzss     LZSS, 1 KB window, 4-bit lengths (heatshrink-like; window + index about 2 KB of RAM)
  o0       adaptive binary range coder, order 0: 255 probabilities (510 B of RAM), the coder the OT section uses
  o1       the same with the previous byte as context: 256 x 255 probabilities (130 KB, reference only)
  o1h      order 1 hashed to 4,096 probabilities (8 KB of RAM)
  o2h      previous two bytes hashed to 8,192 probabilities (16 KB of RAM)
  lzrc     LZ77 over the record itself + the range coder for every decision (otrec.lz_encode, the chosen form,
           record fmt 5; 3.2 KB of RAM). A 16-slot call-site cache in the debug section was also tried: with LZ on
           top it saved 2 to 7 % more at the cost of a second debug format, so it was left out.

Call sites are picked uniformly from the image's sites (pessimistic) or by a Zipf law, s = 1.1 (a few sites
print most lines, as with OT logging on). Argument values are random, so all sizes are on the high side.

The OT section is already range-coded; every candidate compresses the whole payload, and a record never gets
larger: when the compressed form is not smaller, the raw form goes. Sizes are exact for raw/zlib/lzss and the
coded length of the real range coder for o0/o1/o1h/o2h.
"""
import argparse, os, subprocess, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import esp_image, otrec, proof_all_streams as pas  # noqa: E402


class Rc:
    """LZMA-style binary range encoder: 12-bit probabilities, shift 4 (as crec::RcEnc)."""

    def __init__(self):
        self.low, self.rng, self.cache, self.csize, self.out = 0, 0xFFFFFFFF, 0, 1, bytearray()

    def _shift(self):
        if self.low < 0xFF000000 or self.low >= 1 << 32:
            carry = self.low >> 32
            t = self.cache
            while True:
                self.out.append((t + carry) & 0xFF)
                t = 0xFF
                self.csize -= 1
                if not self.csize:
                    break
            self.cache = (self.low >> 24) & 0xFF
        self.csize += 1
        self.low = (self.low << 8) & 0xFFFFFFFF

    def bit(self, probs, i, b, shift=4):
        p = probs[i]
        bound = (self.rng >> 12) * p
        if b:
            self.low += bound
            self.rng -= bound
            probs[i] = p - (p >> shift)
        else:
            self.rng = bound
            probs[i] = p + ((4096 - p) >> shift)
        while self.rng < 1 << 24:
            self.rng = (self.rng << 8) & 0xFFFFFFFF
            self._shift()

    def done(self):
        for _ in range(5):
            self._shift()
        return bytes(self.out)


def cm(data, ctx_bits, ctx_of):
    """Bytewise binary context model: 8 bits MSB first through a bit tree; context selects a table."""
    probs = [2048] * ((1 << ctx_bits) * 256)
    rc = Rc()
    for i, byte in enumerate(data):
        base = (ctx_of(data, i) & ((1 << ctx_bits) - 1)) * 256
        node = 1
        for k in range(7, -1, -1):
            b = (byte >> k) & 1
            rc.bit(probs, base + node, b)
            node = node * 2 + b
    return rc.done()


def o0(d):
    return cm(d, 0, lambda d, i: 0)


def o1(d):
    return cm(d, 8, lambda d, i: d[i - 1] if i else 0)


def o1h(d):                                   # 4,096 probabilities = 16 contexts x 256 nodes
    return cm(d, 4, lambda d, i: (d[i - 1] * 0x9E37 >> 12) if i else 0)


def o2h(d):                                   # 8,192 probabilities = 32 contexts x 256 nodes
    return cm(d, 5, lambda d, i: ((d[i - 1] << 8 | (d[i - 2] if i > 1 else 0)) * 0x9E3779B1 >> 27) if i else 0)


def lzss(data, wbits=10, lbits=4, minm=3):
    """Greedy LZSS bit count: flag + literal 8, or flag + offset wbits + length lbits."""
    bits, i, n, maxm = 0, 0, len(data), (1 << lbits) + minm - 1
    while i < n:
        best, lo = 0, max(0, i - (1 << wbits))
        for j in range(lo, i):
            m = 0
            while m < maxm and i + m < n and data[j + m] == data[i + m]:
                m += 1
            if m > best:
                best = m
        if best >= minm:
            bits += 1 + wbits + lbits
            i += best
        else:
            bits += 9
            i += 1
    return (bits + 7) // 8


def sections(payload):
    out, i = {}, 0
    while i < len(payload):
        t = payload[i]
        i += 1
        n, sh = 0, 0
        while True:
            b = payload[i]
            i += 1
            n |= (b & 0x7F) << sh
            sh += 7
            if not b & 0x80:
                break
        out[t] = out.get(t, 0) + n
        i += n
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--minutes", type=float, default=30)
    ap.add_argument("--work", default="/tmp/compress-study")
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    exe = os.path.join(a.work, "ref_printf")
    subprocess.run(["cc", "-O2", "-o", exe, os.path.join(HERE, "ref_printf.c")], check=True)
    image = esp_image.EspImage(a.image)
    coders = [("zlib9", lambda d: zlib.compress(d, 9)), ("lzss", None), ("o0", o0), ("o1h", o1h), ("o2h", o2h),
              ("o1", o1), ("lzrc", otrec.lz_encode)]
    for profile, sites in (("no-psram", "uniform"), ("psram", "uniform"), ("no-psram", "zipf"), ("psram", "zipf")):
        frames, sat_ref, dbg_events, _, _ = pas.build(image, exe, a.minutes, profile, site_dist=sites)
        records = pas.make_records(frames, sat_ref, dbg_events, "no-psram")      # 4 KB records, no zlib
        label = ("debug ~0.3 KB/s" if profile == "no-psram" else "full debug ~3 KB/s") + f", call sites {sites}"
        raw = sum(len(p) for _, p in records)
        sec = {}
        for _, p in records:
            for k, v in sections(p).items():
                sec[k] = sec.get(k, 0) + v
        print(f"\n{label}: {len(records)} records in {a.minutes:g} min, raw {raw} B "
              f"(OT {sec.get(otrec.SEC_OT_V2, 0) + sec.get(otrec.SEC_OT, 0)}, SAT {sec.get(otrec.SEC_SAT, 0)}, "
              f"debug {sec.get(otrec.SEC_DBG, 0)})")
        for name, f in coders:
            tot = 0
            for _, p in records:
                c = lzss(p) if name == "lzss" else len(f(p))
                tot += min(c + 1, len(p) + 1)                      # 1 flag byte: compressed or raw
            print(f"  {name:8s} {tot:8d} B  {100 * tot / raw:5.1f} % of raw")


if __name__ == "__main__":
    main()
