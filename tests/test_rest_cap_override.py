#!/usr/bin/env python3
"""TASK-3091: the REST in-flight cap can be overridden at runtime in prerelease builds only (debug flag "restcap", telnet key R).

Compiles the REAL cap code cut out of restAPI.ino (REST_MAX_INFLIGHT, restCapBase(), restEffectiveInflightCap()) and the REAL
parse helpers of debugFlags.h, against stubs for the largest free block and state.debug.iRestCap, twice: with
_VERSION_PRERELEASE defined and without it.
  P1  prerelease, default iRestCap 2: the effective cap is 2 where the heap allows (no change as shipped)
  P2  prerelease, iRestCap 6: 6 where the largest block is 24000 or more, 2 between 16000 and 24000, 1 below 16000 (the heap-tier
      clamp stays)
  P3  prerelease, out of range values clamp to 2..8 (0 gives 2, 12 gives 8)
  S1  stable (no _VERSION_PRERELEASE): iRestCap is ignored, the cap stays REST_MAX_INFLIGHT (2)
  H1  debugRestCapParse(): "2".."8" accepted, everything else (null, "", "1", "9", "10", "abc", " 3") refused with 0
  H2  debugRestCapNext(): 2 3 4 5 6 7 8 then back to 2; an out-of-range value goes to 2
Red on the base (OTGW_SRC_REV, e.g. origin/dev): no override: P2 sees 2 instead of 6.
Mutants (each must turn a test red): override ignored; clamp removed; the stable honours the override.
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_mqtt_state import read  # noqa: E402
from test_sat_published_curve import compile_cpp  # noqa: E402

PRELUDE = r"""
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <atomic>
static uint32_t g_mb = 100000;
static uint32_t platformMaxFreeBlock() { return g_mb; }
static struct { struct { uint8_t iRestCap = 2; } debug; } state;
"""

MAIN = r"""
static void line(const char* tag, uint32_t mb, uint8_t cap) {
  state.debug.iRestCap = cap; g_mb = mb;
  printf("%s mb=%u cap=%u eff=%u\n", tag, (unsigned)mb, (unsigned)cap, (unsigned)restEffectiveInflightCap());
}
int main() {
  line("p1", 30000, 2);
  line("p2a", 30000, 6); line("p2b", 20000, 6); line("p2c", 15000, 6);
  line("p3a", 30000, 0); line("p3b", 30000, 12); line("p3c", 30000, 8);
  const char* bad[] = {nullptr, "", "1", "9", "10", "abc", " 3", "2x"};
  int refused = 0;
  for (const char* b : bad) if (debugRestCapParse(b) == 0) ++refused;
  printf("h1 refused=%d ok2=%u ok5=%u ok8=%u\n", refused, (unsigned)debugRestCapParse("2"), (unsigned)debugRestCapParse("5"), (unsigned)debugRestCapParse("8"));
  printf("h2 n2=%u n5=%u n8=%u n0=%u n12=%u\n", (unsigned)debugRestCapNext(2), (unsigned)debugRestCapNext(5), (unsigned)debugRestCapNext(8),
         (unsigned)debugRestCapNext(0), (unsigned)debugRestCapNext(12));
  return 0;
}
"""

HELPERS_FALLBACK = """
static inline uint8_t debugRestCapParse(const char*) { return 0; }
static inline uint8_t debugRestCapNext(uint8_t) { return 2; }
"""


def build(prerelease, transform=None, tag="x"):
    api = read("restAPI.ino")
    a = api.index("#ifndef REST_MAX_INFLIGHT")
    b = api.index("// ADR-147 D4.1: backpressure gate", a)
    cap = api[a:b]
    flags = read("debugFlags.h")
    helpers = ""
    m = re.search(r"static const uint8_t kRestCapMin.*?\n}\n.*?static inline uint8_t debugRestCapNext.*?\n}\n", flags, re.S)
    helpers = m.group(0) if m else HELPERS_FALLBACK
    head = "#define _VERSION_PRERELEASE beta.1\n" if prerelease else ""
    text = head + PRELUDE + helpers + "\n" + cap
    if "restCapBase" not in cap:   # the base: no override, the cap is the macro
        text += "\nstatic inline uint8_t restCapBase() { return REST_MAX_INFLIGHT; }\n"
    if transform:
        new = transform(text)
        assert new != text, "mutant changed nothing"
        text = new
    tmp = tempfile.TemporaryDirectory()
    src = Path(tmp.name) / f"cap_{tag}.cpp"
    src.write_text(text + "\n" + MAIN, encoding="utf-8")
    exe = compile_cpp(src, Path(tmp.name) / f"cap_{tag}.exe")
    out = {}
    if exe:
        res = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        for ln in res.splitlines():
            name, *rest = ln.split()
            out[name] = dict(x.split("=", 1) for x in rest)
    return tmp, out


class RestCapOverride(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp_p, cls.pre = build(True, tag="pre")
        cls.tmp_s, cls.stable = build(False, tag="stable")
        print("\n" + "\n".join(f"pre {k} {v}" for k, v in cls.pre.items()))
        print("\n".join(f"stable {k} {v}" for k, v in cls.stable.items()))

    @classmethod
    def tearDownClass(cls):
        cls.tmp_p.cleanup()
        cls.tmp_s.cleanup()

    def need(self):
        if not self.pre:
            self.skipTest("no host C++ compiler")

    def test_p1_default_is_the_shipped_cap(self):
        self.need()
        self.assertEqual(self.pre["p1"]["eff"], "2")

    def test_p2_override_raises_the_cap_but_the_heap_tier_clamp_stays(self):
        self.need()
        self.assertEqual((self.pre["p2a"]["eff"], self.pre["p2b"]["eff"], self.pre["p2c"]["eff"]), ("6", "2", "1"))

    def test_p3_out_of_range_values_clamp(self):
        self.need()
        self.assertEqual((self.pre["p3a"]["eff"], self.pre["p3b"]["eff"], self.pre["p3c"]["eff"]), ("2", "8", "8"))

    def test_s1_the_stable_ignores_the_override(self):
        self.need()
        for k in ("p2a", "p3b", "p3c"):
            self.assertEqual(self.stable[k]["eff"], "2", k)

    def test_h1_parse(self):
        self.need()
        if self.pre["h1"]["ok5"] == "0":
            self.skipTest("the base has no parse helper")
        self.assertEqual(self.pre["h1"], {"refused": "8", "ok2": "2", "ok5": "5", "ok8": "8"})

    def test_h2_next_cycles_2_to_8(self):
        self.need()
        if self.pre["h2"]["n2"] == "2":
            self.skipTest("the base has no cycle helper")
        self.assertEqual(self.pre["h2"], {"n2": "3", "n5": "6", "n8": "2", "n0": "2", "n12": "2"})

    def mutant(self, name, prerelease, transform, check):
        tmp, out = build(prerelease, transform, name)
        try:
            if not out:
                self.skipTest("no host C++ compiler")
            self.assertTrue(check(out), f"mutant {name} still passes: {out}")
        finally:
            tmp.cleanup()

    def test_mutant_override_ignored(self):
        self.mutant("ignored", True, lambda t: t.replace("const uint8_t n = state.debug.iRestCap;", "const uint8_t n = 2;"),
                    lambda o: o["p2a"]["eff"] != "6")

    def test_mutant_clamp_removed(self):
        self.mutant("noclamp", True, lambda t: t.replace("((n > 8) ? 8 : n)", "n"), lambda o: o["p3b"]["eff"] != "8")

    def test_mutant_stable_honours_the_override(self):
        self.mutant("stablehonours", False,
                    lambda t: t.replace("static inline uint8_t restCapBase() { return REST_MAX_INFLIGHT; }",
                                        "static inline uint8_t restCapBase() { return state.debug.iRestCap; }"),
                    lambda o: o["p2a"]["eff"] != "2")


if __name__ == "__main__":
    unittest.main(verbosity=2)
