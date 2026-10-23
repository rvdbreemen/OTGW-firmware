#!/usr/bin/env python3
"""TASK-1337 guard: ot_log_buffer has one writer, the loop task (SAT narration + SAT-SIM rejection).

sendEventToWebSocket() fills the global ot_log_buffer. satNarrate*() runs on the loop task AND on
async_tcp (satHandleControlMode, satSimInjectEvent, satClearSafetyTrip via a settings POST), and the
SATsimulation rejection in updateSetting() runs on async_tcp. They post to a small ring
(satWsEventPost, portMUX-guarded); satControlLoop() drains it on the loop task (satWsEventDrain).

Source checks:
  1. satNarrate_P / satNarratef_P / satPostWsEvent_P do not call sendEventToWebSocket; they post.
  2. settingStuff.ino sends no WS event directly.
  3. satControlLoop() drains the ring before the control pass.
  4. The drain is the only sendEventToWebSocket call in the ring/narration block.
Host check: the real code between SAT_WS_RING_BEGIN / SAT_WS_RING_END is compiled with g++ (portMUX
stubbed) and run: FIFO order, overflow drops the oldest and counts it, truncation, two threads posting.

Not checked: that the firmware compiles, or that WS frames arrive on a device.
OTGW_SAT_INO / OTGW_SETTING_INO point at older copies to see it red
(`git show HEAD:src/OTGW-firmware/SATcontrol.ino` is the unfixed version).
"""
import os, re, shutil, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src", "OTGW-firmware")
SAT = os.environ.get("OTGW_SAT_INO", os.path.join(SRC, "SATcontrol.ino"))
SET = os.environ.get("OTGW_SETTING_INO", os.path.join(SRC, "settingStuff.ino"))


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read().replace("\r\n", "\n")


def body(text, signature):
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


class Source(unittest.TestCase):
    def test_narration_posts_instead_of_sending(self):
        sat = read(SAT)
        for sig in ("void satNarrate_P(PGM_P msg_P)", "void satNarratef_P(PGM_P fmt_P, ...)"):
            b = body(sat, sig)
            self.assertNotRegex(b, r"sendEventToWebSocket", sig + " writes ot_log_buffer")
            self.assertIn("satWsEventPost(", b)

    def test_settings_rejection_posts(self):
        st = read(SET)
        self.assertNotRegex(st, r"sendEventToWebSocket", "settingStuff.ino (async_tcp) writes ot_log_buffer")
        self.assertIn("satPostWsEvent_P('!'", st)

    def test_loop_drains_before_control_pass(self):
        b = body(read(SAT), "void satControlLoop()")
        self.assertIn("satWsEventDrain()", b)
        self.assertLess(b.index("satWsEventDrain()"), b.index("satControlLoopRun()"))

    def test_only_drain_sends(self):
        sat = read(SAT)
        a = sat.index("#define SAT_NARRATE_BUF")
        z = sat.index('#include "SATchEnable.h"')
        block = sat[a:z]
        d = body(block, "static void satWsEventDrain()")
        rest = block.replace(d, "")
        self.assertNotRegex(re.sub(r"//.*", "", rest), r"sendEventToWebSocket")


HOST_MAIN = r'''
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <thread>
#include <atomic>
#define SAT_NARRATE_BUF 96
typedef int portMUX_TYPE; // replaced below by a real mutex
#include <mutex>
static std::mutex g_mu;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) g_mu.lock()
#define portEXIT_CRITICAL(x) g_mu.unlock()
static size_t strlcpy(char *d, const char *s, size_t n){ size_t l=strlen(s); if(n){size_t c=l>=n?n-1:l; memcpy(d,s,c); d[c]=0;} return l; }
#include "ring.inc"
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static bool takeE(SatWsEvent &e, uint16_t &d) { return satWsEventTake(e.prefix, e.msg, sizeof(e.msg), d); }
int main() {
  SatWsEvent e; uint16_t dr;
  CHECK(!takeE(e, dr));
  satWsEventPost('S', "one"); satWsEventPost('!', "two");
  CHECK(takeE(e, dr) && e.prefix=='S' && !strcmp(e.msg,"one") && dr==0);
  CHECK(takeE(e, dr) && e.prefix=='!' && !strcmp(e.msg,"two"));
  CHECK(!takeE(e, dr));
  // overflow: 6 posts into a ring of 4 -> oldest 2 dropped, newest 4 kept in order
  for (int i = 0; i < 6; i++) { char b[8]; snprintf(b, sizeof b, "m%d", i); satWsEventPost('S', b); }
  CHECK(takeE(e, dr) && dr == 2 && !strcmp(e.msg, "m2"));
  for (int i = 3; i < 6; i++) { char b[8]; snprintf(b, sizeof b, "m%d", i); CHECK(takeE(e, dr) && dr==0 && !strcmp(e.msg, b)); }
  CHECK(!takeE(e, dr));
  // truncation
  char big[300]; memset(big, 'x', 299); big[299] = 0;
  satWsEventPost('S', big);
  CHECK(takeE(e, dr) && strlen(e.msg) == SAT_NARRATE_BUF - 1);
  // two producers, one consumer: every taken entry is intact (prefix matches body letter)
  std::atomic<bool> stop(false); std::atomic<int> bad(0), got(0);
  auto prod = [&](char c) { char m[40]; memset(m, c, 39); m[39]=0; for (int i=0;i<200000;i++) satWsEventPost(c, m); };
  std::thread a(prod, 'A'), b(prod, 'B');
  std::thread cons([&]{ SatWsEvent x; uint16_t d; while(!stop){ if(takeE(x,d)){ got++; for(int i=0;i<39;i++) if(x.msg[i]!=x.prefix) {bad++; break;} } } });
  a.join(); b.join(); stop = true; cons.join();
  printf("taken=%d bad=%d\n", got.load(), bad.load());
  CHECK(bad == 0 && got > 0);
  printf(fails ? "HOST FAIL\n" : "HOST OK\n");
  return fails ? 1 : 0;
}
'''


def _vcvars():
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                          "Microsoft Visual Studio", "Installer", "vswhere.exe")
    if not os.path.exists(vswhere):
        return None
    out = subprocess.run([vswhere, "-latest", "-products", "*", "-requires",
                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                          "-property", "installationPath"], capture_output=True, text=True).stdout.strip()
    bat = os.path.join(out, "VC", "Auxiliary", "Build", "vcvars64.bat") if out else None
    return bat if bat and os.path.exists(bat) else None


def compile_cpp(cpp, exe, d):
    for cc in ("g++", "clang++"):
        if shutil.which(cc):
            r = subprocess.run([cc, "-std=c++17", "-O1", "-pthread", "-I", d, "-o", exe, cpp],
                               capture_output=True, text=True)
            assert r.returncode == 0, r.stderr
            return True
    bat = _vcvars()
    if bat:
        r = subprocess.run('call "%s" >nul && cl /nologo /std:c++17 /EHsc /O1 /I"%s" /Fe:"%s" /Fo:"%s\\\\" "%s"'
                           % (bat, d, exe, d, cpp), shell=True, cwd=d, capture_output=True, text=True)
        assert r.returncode == 0, r.stdout[-3000:] + r.stderr[-1000:]
        return True
    return False


class Host(unittest.TestCase):
    def test_real_ring_code_compiles_and_behaves(self):
        sat = read(SAT)
        if "SAT_WS_RING_BEGIN" not in sat:
            self.fail("ring code not found in " + SAT)
        code = sat[sat.index("// SAT_WS_RING_BEGIN"):sat.index("// SAT_WS_RING_END")]
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, "ring.inc"), "w") as f:
                f.write(code)
            with open(os.path.join(d, "main.cpp"), "w") as f:
                f.write(HOST_MAIN)
            exe = os.path.join(d, "t.exe")
            if not compile_cpp(os.path.join(d, "main.cpp"), exe, d):
                self.skipTest("no host C++ compiler (g++, clang++, MSVC)")
            r = subprocess.run([exe], capture_output=True, text=True, timeout=120)
            print(r.stdout)
            self.assertEqual(r.returncode, 0, r.stdout)


if __name__ == "__main__":
    unittest.main()
