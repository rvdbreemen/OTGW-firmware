#!/usr/bin/env python3
"""TASK-1966 / TASK-1980: the OT-Direct boiler line keeps working without a valid boiler answer.

TASK-1966 found it (library src/libraries/OpenTherm/src/OpenTherm.cpp): isReady() also needs
micros() - delayTimestamp < 0x80000000, and delayTimestamp moved only on a valid answer. micros() is 32 bit, so with
no valid answer (no boiler, boiler off) isReady() turned false 2^31 us (35.8 min) after the last valid one and stayed
false for 35.8 min. Seen on .61 (09, beta.298, no boiler). TASK-1966 worked around it in OTDirect.ino; TASK-1980
(Robert, DM 1571553430923911252 and 1571560593712746617) pins the library to rvdbreemen/opentherm_library
fix-timer-wrap (upstream PR Phunkafizer/opentherm_library#1), which fixes it in the library, and drops the
workaround.

The code under test is the REAL library: src/libraries/OpenTherm/src/OpenTherm.cpp compiled unchanged on the host,
with a stub Arduino.h (a fake micros() the test moves) and a stub driver/gptimer.h (the test fires the tx alarm).
It needs a 32-bit unsigned long, as on the ESP32: MSVC (LLP64). process() runs every millisecond, as loopOTDirect().

Cases: (1) no answer for about 77 min, one request every 1.5 s: never refused, past 2^31 and 2^32 us;
(2) setDelay(10) 1 ms before the micros() wrap is kept; (3) end() + begin() during a frame leaves the line ready
(also the GW=R path, TASK-1978); (4) a tx timer that stops mid-frame ends the frame with TIMEOUT.
Old-vs-fix: test_old_pin_is_red compiles the former pin f7d8254 from the submodule's history and expects case 1 to
fail there; the structural tests check that OTDirect.ino uses the library directly.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import _vcvars  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
LIB_DIR = REPO / "src" / "libraries" / "OpenTherm"
OTD_INO = REPO / "src" / "OTGW-firmware" / "OTDirect.ino"
OLD_PIN = "f7d8254"

ARDUINO_H = r"""#pragma once
#include <stdint.h>
#include <stddef.h>
#define SOC_GPTIMER_SUPPORTED 1
#define IRAM_ATTR
#define ICACHE_RAM_ATTR
typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define CHANGE 3
#define bitRead(v, b) (((v) >> (b)) & 1UL)
static_assert(sizeof(unsigned long) == 4, "the library assumes a 32-bit unsigned long, as on the ESP32");
extern unsigned long g_us;
inline unsigned long micros() { return g_us; }
inline void delay(unsigned long ms) { g_us += ms * 1000UL; }
inline void delayMicroseconds(unsigned int us) { g_us += us; }
inline void yield() {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 0; }
inline int digitalPinToInterrupt(int p) { return p; }
inline void attachInterrupt(int, void (*)(void), int) {}
inline void attachInterruptArg(int, void (*)(void*), void*, int) {}
inline void detachInterrupt(int) {}
inline void noInterrupts() {}
inline void interrupts() {}
"""

GPTIMER_H = r"""#pragma once
typedef int esp_err_t;
#define ESP_OK 0
struct gptimer_t;
typedef gptimer_t *gptimer_handle_t;
typedef enum { GPTIMER_CLK_SRC_DEFAULT } gptimer_clock_source_t;
typedef enum { GPTIMER_COUNT_UP } gptimer_count_direction_t;
typedef struct { gptimer_clock_source_t clk_src; gptimer_count_direction_t direction; uint32_t resolution_hz; } gptimer_config_t;
typedef struct { uint64_t count_value; uint64_t alarm_value; } gptimer_alarm_event_data_t;
typedef bool (*gptimer_alarm_cb_t)(gptimer_handle_t, const gptimer_alarm_event_data_t *, void *);
typedef struct { gptimer_alarm_cb_t on_alarm; } gptimer_event_callbacks_t;
typedef struct { uint64_t alarm_count; uint64_t reload_count; struct { uint32_t auto_reload_on_alarm; } flags; } gptimer_alarm_config_t;
struct gptimer_t { gptimer_alarm_cb_t cb; void *user; bool running; };
extern gptimer_t g_timer;
inline esp_err_t gptimer_new_timer(const gptimer_config_t *, gptimer_handle_t *h) { g_timer = gptimer_t{}; *h = &g_timer; return ESP_OK; }
inline esp_err_t gptimer_register_event_callbacks(gptimer_handle_t t, const gptimer_event_callbacks_t *c, void *u) { t->cb = c->on_alarm; t->user = u; return ESP_OK; }
inline esp_err_t gptimer_enable(gptimer_handle_t) { return ESP_OK; }
inline esp_err_t gptimer_disable(gptimer_handle_t) { return ESP_OK; }
inline esp_err_t gptimer_del_timer(gptimer_handle_t t) { t->running = false; t->cb = nullptr; return ESP_OK; }
inline esp_err_t gptimer_set_alarm_action(gptimer_handle_t, const gptimer_alarm_config_t *) { return ESP_OK; }
inline esp_err_t gptimer_start(gptimer_handle_t t) { t->running = true; return ESP_OK; }
inline esp_err_t gptimer_stop(gptimer_handle_t t) { t->running = false; return ESP_OK; }
"""

MAIN_CPP = r"""#include "OpenTherm.h"
#include <cstdio>
unsigned long g_us = 1000;
gptimer_t g_timer;
struct Probe : OpenTherm {
    Probe() : OpenTherm(4, 5, false) {}
    int tx() const { return (int)txStatus; }
    void forceDelayTs(unsigned long v) { delayTimestamp = v; }
    void delayMs(uint16_t ms) { setDelay(ms); }
};
// Static, as otMaster is on the device: the library leaves delayTimestamp out of its constructor, so only
// zero-initialised storage gives the same start state.
static Probe p1, p2, p3, p4;
static int g_timeouts = 0;
static void onResp(unsigned long, OpenThermResponseStatus s) { if (s == OpenThermResponseStatus::TIMEOUT) g_timeouts++; }
static void runTimer(int ticks) {
    for (int i = 0; i < ticks && g_timer.running && g_timer.cb; i++) { g_us += 500; g_timer.cb(&g_timer, nullptr, g_timer.user); }
}
static void loopFor(Probe &ot, unsigned long ms) { for (unsigned long i = 0; i < ms; i++) { g_us += 1000; ot.process(); } }
int main() {
    { Probe &ot = p1; ot.begin(nullptr, onResp); g_timeouts = 0;
      unsigned long refusedAt = 0; int sent = 0, refused = 0, wraps = 0;
      for (int s = 0; s < 3000; s++) {
          const unsigned long before = g_us;
          if (ot.sendRequestAsync(0x00000000UL)) { sent++; runTimer(80); }
          else { refused++; if (!refusedAt) refusedAt = g_us; }
          loopFor(ot, 1500);
          if (g_us < before) wraps++;
      }
      printf("c1 sent %d refused %d first %lu wraps %d\n", sent, refused, refusedAt, wraps); }
    { Probe &ot = p2; ot.begin(nullptr, onResp);
      g_us = 0xFFFFFC18UL; ot.forceDelayTs(g_us - 100000UL); ot.delayMs(10);
      const int waiting = !ot.isReady(); loopFor(ot, 11);
      printf("c2 waiting %d ready_after %d\n", waiting, (int)ot.isReady()); }
    { Probe &ot = p3; ot.begin(nullptr, onResp);
      ot.sendRequestAsync(0x00000000UL); runTimer(10); ot.end(); ot.begin(nullptr, onResp); loopFor(ot, 2000);
      printf("c3 tx %d ready %d\n", ot.tx(), (int)ot.isReady()); }
    { Probe &ot = p4; ot.begin(nullptr, onResp); g_timeouts = 0;
      ot.sendRequestAsync(0x00000000UL); runTimer(10); g_timer.running = false; loopFor(ot, 2000);
      printf("c4 tx %d ready %d timeouts %d\n", ot.tx(), (int)ot.isReady(), g_timeouts); }
    return 0;
}
"""


def run_library(cpp_text, h_text):
    """Compile the given OpenTherm.cpp/.h with the stubs and return {case: {key: int}}, or None without MSVC."""
    bat = _vcvars()
    if not bat:
        return None
    work = Path(tempfile.mkdtemp(prefix="otlib_"))
    try:
        (work / "stub" / "driver").mkdir(parents=True)
        (work / "lib").mkdir()
        (work / "stub" / "Arduino.h").write_text(ARDUINO_H, encoding="utf-8")
        (work / "stub" / "FunctionalInterrupt.h").write_text("#pragma once\n#include <functional>\n", encoding="utf-8")
        (work / "stub" / "driver" / "gptimer.h").write_text(GPTIMER_H, encoding="utf-8")
        (work / "lib" / "OpenTherm.cpp").write_text(cpp_text, encoding="utf-8")
        (work / "lib" / "OpenTherm.h").write_text(h_text, encoding="utf-8")
        (work / "main.cpp").write_text(MAIN_CPP, encoding="utf-8")
        exe = work / "t.exe"
        cmd = (f'call "{bat}" >nul && cl /nologo /std:c++20 /EHsc /I"{work / "stub"}" /I"{work / "lib"}" '
               f'/Fe:"{exe}" /Fo:"{work}\\\\" "{work / "main.cpp"}" "{work / "lib" / "OpenTherm.cpp"}"')
        r = subprocess.run(cmd, shell=True, cwd=work, capture_output=True, text=True)
        if r.returncode:
            raise AssertionError("host compile failed:\n" + r.stdout[-3000:] + r.stderr[-1000:])
        out = subprocess.run([str(exe)], capture_output=True, text=True, timeout=300).stdout
        res = {}
        for line in out.splitlines():
            parts = line.split()
            res[parts[0]] = {parts[i]: int(parts[i + 1]) for i in range(1, len(parts) - 1, 2)}
        return res
    finally:
        shutil.rmtree(work, ignore_errors=True)


def read_lib(name):
    return (LIB_DIR / "src" / name).read_text(encoding="utf-8")


class LibraryLine(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = run_library(read_lib("OpenTherm.cpp"), read_lib("OpenTherm.h"))
        if cls.r is None:
            raise unittest.SkipTest("needs MSVC (32-bit unsigned long, as on the ESP32)")

    def test_1_no_answer_never_refused_past_both_wraps(self):
        c = self.r["c1"]
        self.assertEqual(c["refused"], 0, c)
        self.assertEqual(c["sent"], 3000, c)
        self.assertEqual(c["wraps"], 1, "the run must pass the 2^32 us wrap of micros()")

    def test_2_set_delay_before_the_wrap_is_kept(self):
        self.assertEqual(self.r["c2"], {"waiting": 1, "ready_after": 1})

    def test_3_end_begin_during_a_frame_leaves_the_line_ready(self):
        self.assertEqual(self.r["c3"], {"tx": 0, "ready": 1})

    def test_4_stalled_tx_timer_ends_the_frame_with_timeout(self):
        self.assertEqual(self.r["c4"], {"tx": 0, "ready": 1, "timeouts": 1})


class OldPin(unittest.TestCase):
    def test_old_pin_is_red(self):
        def show(name):
            r = subprocess.run(["git", "-C", str(LIB_DIR), "show", f"{OLD_PIN}:src/{name}"],
                               capture_output=True, text=True, encoding="utf-8")
            return r.stdout if r.returncode == 0 else None
        cpp, h = show("OpenTherm.cpp"), show("OpenTherm.h")
        if cpp is None or h is None:
            self.skipTest(f"{OLD_PIN} not in the submodule's object store")
        r = run_library(cpp, h)
        if r is None:
            self.skipTest("needs MSVC")
        c1 = r["c1"]
        self.assertGreater(c1["refused"], 0, "the old pin must show the stall")
        self.assertGreaterEqual(c1["first"], 0x80000000 - 2000000)   # within 2 s of 2^31 us
        self.assertLessEqual(c1["first"], 0x80000000 + 2000000)
        self.assertEqual(r["c3"]["ready"], 0, "the old pin stays in TX_DATA after end()+begin()")


class Wiring(unittest.TestCase):
    def test_otdirect_uses_the_library_directly(self):
        src = OTD_INO.read_text(encoding="utf-8")
        self.assertIn("static OpenTherm otMaster(PIN_OT_MASTER_IN, PIN_OT_MASTER_OUT);", src)
        self.assertNotIn("OTDMasterLine", src)
        self.assertNotIn("keepFresh", src)

    def test_submodule_points_at_the_fork(self):
        gm = (REPO / ".gitmodules").read_text(encoding="utf-8")
        m = re.search(r'\[submodule "src/libraries/OpenTherm"\][^\[]*?url\s*=\s*(\S+)', gm)
        self.assertIsNotNone(m)
        self.assertEqual(m.group(1), "https://github.com/rvdbreemen/opentherm_library")
        self.assertIn("TX_STALL_TIMEOUT_US", read_lib("OpenTherm.cpp"), "the pinned library must carry the fix")


if __name__ == "__main__":
    unittest.main()
