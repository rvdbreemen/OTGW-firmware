#!/usr/bin/env python3
"""TASK-1972: the record compressor of the cloud recorder (crec::lzEncode in src/OTGW-firmware/CloudRecCodec.h).

The real header is compiled with the host C++ compiler; nothing is re-implemented here.
  1. On records from the cloudrec harness (a 30-minute run with OT, SATTICK and debug lines) and on edge cases
     (empty, 1-3 bytes, random, long runs, a record of the maximum size), lzEncode gives the same bytes as the
     reference otrec.lz_encode, and otrec.lz_decode gives every record back exactly.
  2. The counting sink (Buf::initSink) counts exactly the bytes the memory form writes.
  3. On the harness records the compressed form is smaller in total.
"""
import os, random, shutil, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools", "cloud-recorder"))
sys.path.insert(0, os.path.join(ROOT, "tests"))
import otrec  # noqa: E402

SRC = r'''
#include <cstdio>
#include <cstring>
#include <vector>
#include "@HDR@"
static crec::LzWork w;
static void noop(void *, uint8_t) {}
int main() {
  static char line[1 << 16];
  while (fgets(line, sizeof line, stdin)) {
    std::vector<uint8_t> in;
    for (char *h = line; h[0] && h[1] && h[0] != '\n'; h += 2) { unsigned v; sscanf(h, "%2x", &v); in.push_back((uint8_t)v); }
    static uint8_t out[16384];
    crec::Buf b; b.init(out, sizeof out);
    crec::lzEncode(in.data(), in.size(), b, w);
    crec::Buf c; c.initSink(noop, nullptr);
    crec::lzEncode(in.data(), in.size(), c, w);
    printf("%zu %d ", c.n, (int)b.full);
    for (size_t i = 0; i < b.n; i++) printf("%02x", out[i]);
    printf("\n");
  }
}
'''.replace("@HDR@", os.path.join(ROOT, "src", "OTGW-firmware", "CloudRecCodec.h").replace("\\", "/"))


def build():
    cxx = shutil.which("g++") or shutil.which("c++") or shutil.which("clang++")
    if not cxx:
        return None
    d = tempfile.mkdtemp(prefix="crlz-")
    src, exe = os.path.join(d, "lz.cpp"), os.path.join(d, "lz")
    open(src, "w").write(SRC)
    subprocess.run([cxx, "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror", "-o", exe, src], check=True)
    return exe


def harness_records():
    import test_cloudrec_codec as t
    exe = t.Harness.build()
    if not exe:
        return []
    t.Harness.exe = exe
    lines = t.build_events(30, 300)[0]
    return [p for _, p in t.Harness.run(lines)]


EXE = build()


@unittest.skipIf(EXE is None, "no C++ compiler")
class CloudRecLzTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rnd = random.Random(5)
        edge = [b"", b"a", b"ab", b"abc", b"aaaa", bytes(300), bytes(range(256)) * 3, os.urandom(1000),
                bytes(rnd.choice(b"abc\x00\xff") for _ in range(4112)), b"xyz" * 1400]
        cls.recs = harness_records()
        cls.inputs = edge + cls.recs
        r = subprocess.run([EXE], input="\n".join(x.hex() for x in cls.inputs) + "\n", capture_output=True,
                           text=True, check=True)
        cls.out = [l.split(" ") for l in r.stdout.splitlines()]

    def test_same_bytes_as_reference_and_lossless(self):
        self.assertGreater(len(self.recs), 30)
        self.assertEqual(len(self.out), len(self.inputs))
        for raw, (count, full, hx) in zip(self.inputs, self.out):
            got = bytes.fromhex(hx)
            self.assertEqual(full, "0")
            self.assertEqual(got.hex(), otrec.lz_encode(raw).hex(), f"input of {len(raw)} B")
            self.assertEqual(otrec.lz_decode(got), raw)

    def test_counting_sink(self):
        for count, _, hx in self.out:
            self.assertEqual(int(count), len(hx) // 2)

    def test_harness_records_get_smaller(self):
        n = len(self.inputs) - len(self.recs)
        raw = sum(len(x) for x in self.recs)
        lz = sum(min(len(hx) // 2, len(x)) for x, (_, _, hx) in zip(self.recs, self.out[n:]))
        print(f"\nharness records: {len(self.recs)}, raw {raw} B, sent {lz} B ({100 * lz / raw:.1f} %)")
        self.assertLess(lz, raw)


if __name__ == "__main__":
    unittest.main(verbosity=2)
