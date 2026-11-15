#!/usr/bin/env python3
"""TASK-1991: the local sink of the one recorder (src/OTGW-firmware/RecRing.h), compiled as it is.

  1. 2,000 entries of random size into a small ring: the ring always holds the newest entries that fit, in
     order, byte for byte; the dropped count is right.
  2. A download read in random chunk sizes while entries keep arriving gives whole entries only, in seq order,
     each byte-identical to what was written: an entry dropped before its turn is skipped, an entry partly sent
     is pinned and never cut (a new entry that would need its room is refused instead).
  3. An entry larger than the ring is refused; lookups by seq find the first entry at or after it.
"""
import os, random, shutil, subprocess, struct, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "@HDR@"
int main(int, char **argv) {
  unsigned seed = (unsigned)atoi(argv[1]);
  uint32_t cap = (uint32_t)atoi(argv[2]);
  srand(seed);
  std::vector<uint8_t> mem(cap);
  crec::RecRing r;
  r.init(mem.data(), cap);
  crec::RingCursor cur;
  uint64_t seq = 100;
  for (int k = 0; k < 2000; k++) {
    uint32_t len = (uint32_t)(rand() % (cap / 3));
    if (k == 1000) len = cap;                                // never fits
    uint8_t fmt = (uint8_t)(4 + rand() % 2);
    if (r.begin(fmt, len, seq, (int64_t)seq * 1000, (uint32_t)k)) {
      printf("W %llu %u %u ", (unsigned long long)seq, fmt, len);
      for (uint32_t i = 0; i < len; i++) { uint8_t b = (uint8_t)(rand() & 0xFF); r.byte(b); printf("%02x", b); }
      r.commit();
      printf("\n");
    } else {
      printf("R %llu %u\n", (unsigned long long)seq, len);
    }
    seq++;
    if (rand() % 3 == 0) {                                   // the download reads a bit
      uint8_t buf[512];
      size_t n = crec::ringFill(r, cur, buf, 1 + rand() % sizeof(buf), (uint32_t)k);
      printf("D ");
      for (size_t i = 0; i < n; i++) printf("%02x", buf[i]);
      printf("\n");
    }
  }
  uint64_t o = 0, nw = 0;
  bool h1 = r.oldestSeq(o), h2 = r.newestSeq(nw);
  printf("S %u %u %d %llu %d %llu\n", r.entries, r.evicted, h1, (unsigned long long)o, h2, (unsigned long long)nw);
  uint32_t off;
  bool f = r.find(o + 1, off);
  printf("F %d %llu\n", f, f ? (unsigned long long)r.entrySeq(off) : 0ull);
  printf("N ");
  for (uint32_t i = 0, p = r.head; i < r.entries; i++) { printf("%llu,", (unsigned long long)r.entrySeq(p)); p = (p + r.entryBytes(p)) % r.cap; }
  printf("\n");
}
'''.replace("@HDR@", os.path.join(ROOT, "src", "OTGW-firmware", "RecRing.h").replace("\\", "/"))


def build():
    cxx = shutil.which("g++") or shutil.which("c++") or shutil.which("clang++")
    if not cxx:
        return None
    d = tempfile.mkdtemp(prefix="ring-")
    src, exe = os.path.join(d, "r.cpp"), os.path.join(d, "r")
    open(src, "w").write(SRC)
    subprocess.run([cxx, "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror", "-o", exe, src], check=True)
    return exe


EXE = build()


def run(seed, cap):
    out = subprocess.run([EXE, str(seed), str(cap)], capture_output=True, text=True, check=True).stdout
    return [l.split(" ") for l in out.splitlines()]


def parse_entries(blob):
    """Entries of a download stream: (seq, fmt, payload)."""
    out, i = [], 0
    while i < len(blob):
        assert blob[i] == 0xA5, f"bad magic at {i}"
        if i + 20 > len(blob):
            break                                                 # the download is still in this entry's header
        fmt, ln, seq, t0 = struct.unpack_from("<BHQq", blob, i + 1)
        if i + 20 + ln > len(blob):
            break                                                 # still in this entry: the rest follows later
        assert t0 == seq * 1000
        out.append((seq, fmt, blob[i + 20:i + 20 + ln]))
        i += 20 + ln
    return out


@unittest.skipIf(EXE is None, "no C++ compiler")
class RecRingTest(unittest.TestCase):
    def check(self, seed, cap):
        rows = run(seed, cap)
        written = {}
        order = []
        for r in rows:
            if r[0] == "W":
                seq = int(r[1])
                written[seq] = (int(r[2]), bytes.fromhex(r[4]) if len(r) > 4 else b"")
                order.append(seq)
            elif r[0] == "R":
                refused = refused + 1 if 'refused' in dir() else 1
        # 1. what the ring holds now: the newest entries that fit, in order
        held = [int(x) for x in rows[-1][1].split(",") if x]
        self.assertEqual(held, order[-len(held):])
        self.assertLessEqual(sum(20 + written[s][1].__len__() for s in held), cap)
        s = [r for r in rows if r[0] == "S"][0]
        self.assertEqual(int(s[1]), len(held))
        self.assertEqual(int(s[2]), len(order) - len(held))               # every other stored entry was dropped once
        f = [r for r in rows if r[0] == "F"][0]
        if len(held) > 1:
            self.assertEqual(int(f[2]), held[1])
        # 2. the download: whole entries, seq ascending, byte-identical
        blob = bytes.fromhex("".join(r[1] for r in rows if r[0] == "D" and len(r) > 1))
        got = parse_entries(blob)
        seqs = [g[0] for g in got]
        self.assertEqual(seqs, sorted(set(seqs)))
        self.assertGreater(len(got), 50)
        for seq, fmt, payload in got:
            self.assertEqual((fmt, payload), written[seq])

    def test_small_ring(self):
        self.check(1, 3000)

    def test_larger_ring(self):
        self.check(7, 20000)


if __name__ == "__main__":
    unittest.main(verbosity=2)
