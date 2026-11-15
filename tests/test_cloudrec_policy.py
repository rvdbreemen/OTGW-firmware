#!/usr/bin/env python3
"""TASK-1971: the cloud recorder's backoff schedule and status classification (src/OTGW-firmware/CloudRecPolicy.h).

The real header is compiled into a small C++ program; nothing is re-implemented here.
  1. Without jitter the waits are 5, 10, 20, ... min, capped at 24 h, and stay at 24 h.
  2. With any random value a wait is 100 to 120 % of its base and never above 24 h; the first wait is 5 to 6 min.
  3. HTTP statuses map to OK / REFUSED / AUTH / DOWN; only a 403 whose body holds "otgw-blocked" is BLOCKED.
  4. The send queue: one slot without PSRAM, two with; a full queue loses the record; the oldest goes first.
"""
import os, shutil, subprocess, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = r'''
#include <cstdio>
#include "@HDR@"
int main() {
  const unsigned rnds[] = {0u, 1u, 2000u, 2001u, 4294967295u, 123456789u};
  for (int s = 0; s < 16; s++) for (unsigned r : rnds) printf("B %d %u %u\n", s, r, crec::backoffMs((uint8_t)s, r));
  const int st[] = {0, 200, 400, 401, 403, 404, 429, 500, 502, 503};
  for (int x : st) printf("C %d %d\n", x, (int)crec::classify(x));
  const char *bodies[] = {"{\"message\":\"Gateway blocked (otgw-blocked): telemetry opt-in off.\",\"status\":403}",
                          "{\"message\":\"The request requires valid record authorization token.\",\"status\":403}", ""};
  const int rs[] = {403, 401, 400};
  for (int x : rs) for (int i = 0; i < 3; i++) printf("R %d %d %d\n", x, i, (int)crec::classifyReply(x, bodies[i]));
  printf("R 403 9 %d\n", (int)crec::classifyReply(403, nullptr));
  static uint8_t b0[4], b1[4];
  for (int two = 0; two < 2; two++) {                      // one slot (no PSRAM) and two slots (PSRAM)
    crec::Slot q[2] = {{b0, 0, 0, 0, false}, {two ? b1 : nullptr, 0, 0, 0, false}};
    uint64_t seq = 10;
    int lost = 0;
    for (int step = 0; step < 6; step++) {                 // close three records, send one, close three more
      if (step == 3) { crec::Slot *o = crec::slotOldest(q, 2); printf("S %d sent %llu\n", two, (unsigned long long)o->seq); o->busy = false; }
      crec::Slot *f = crec::slotFree(q, 2);
      if (!f) { lost++; seq++; continue; }
      f->seq = seq++; f->busy = true;
    }
    crec::Slot *o = crec::slotOldest(q, 2);
    printf("S %d lost %d oldest %llu\n", two, lost, (unsigned long long)(o ? o->seq : 0));
  }
}
'''.replace("@HDR@", os.path.join(ROOT, "src", "OTGW-firmware", "CloudRecPolicy.h").replace("\\", "/"))

MIN = 60000
H24 = 24 * 60 * MIN


def run():
    cxx = shutil.which("g++") or shutil.which("c++") or shutil.which("clang++")
    if not cxx:
        return None
    d = tempfile.mkdtemp(prefix="crpol-")
    src, exe = os.path.join(d, "p.cpp"), os.path.join(d, "p")
    open(src, "w").write(SRC)
    subprocess.run([cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-o", exe, src], check=True)
    return subprocess.run([exe], capture_output=True, text=True, check=True).stdout.split("\n")


OUT = run()


@unittest.skipIf(OUT is None, "no C++ compiler")
class CloudRecPolicyTest(unittest.TestCase):
    def rows(self, k):
        return [list(map(int, l.split()[1:])) for l in OUT if l.startswith(k + " ")]

    def test_schedule_without_jitter(self):
        got = [w for s, r, w in self.rows("B") if r == 0]
        want = [min(5 * MIN << s, H24) for s in range(16)]
        self.assertEqual(got, want)
        self.assertEqual(got[:10], [5 * MIN, 10 * MIN, 20 * MIN, 40 * MIN, 80 * MIN, 160 * MIN, 320 * MIN, 640 * MIN,
                                    1280 * MIN, H24])

    def test_jitter_bounds(self):
        for s, r, w in self.rows("B"):
            base = min(5 * MIN << s, H24)
            self.assertGreaterEqual(w, base)
            self.assertLessEqual(w, min(base * 12 // 10, H24))
        first = [w for s, r, w in self.rows("B") if s == 0]
        self.assertTrue(all(5 * MIN <= w <= 6 * MIN for w in first))
        self.assertEqual(max(w for s, r, w in self.rows("B") if s == 0), 6 * MIN)    # r % 2001 == 2000

    def test_classify(self):
        got = {s: o for s, o in self.rows("C")}
        self.assertEqual(got, {0: 3, 200: 0, 400: 1, 401: 2, 403: 2, 404: 3, 429: 3, 500: 3, 502: 3, 503: 3})

    def test_blocked_reply(self):
        got = {(s, i): o for s, i, o in self.rows("R")}
        self.assertEqual(got[(403, 0)], 4)                       # the hook's message: BLOCKED
        self.assertEqual(got[(403, 1)], 2)                       # an ordinary 403: log in again
        self.assertEqual(got[(403, 2)], 2)
        self.assertEqual(got[(403, 9)], 2)                       # no body
        self.assertEqual(got[(401, 0)], 2)                       # only a 403 counts
        self.assertEqual(got[(400, 0)], 1)


class CloudRecSlotTest(unittest.TestCase):
    def test_queue(self):
        rows = [l.split() for l in OUT if l.startswith("S ")]
        got = {(r[1], r[2]): r[3:] for r in rows}
        # one slot: 10 queued, 11 and 12 lost, 10 sent, 13 queued, 14 and 15 lost -> 4 lost, 13 waiting
        self.assertEqual(got[("0", "sent")], ["10"])
        self.assertEqual(got[("0", "lost")], ["4", "oldest", "13"])
        # two slots: 10 and 11 queued, 12 lost, 10 sent, 13 queued, 14 and 15 lost -> 3 lost, 11 is the oldest
        self.assertEqual(got[("1", "sent")], ["10"])
        self.assertEqual(got[("1", "lost")], ["3", "oldest", "11"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
