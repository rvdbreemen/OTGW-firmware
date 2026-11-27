#!/usr/bin/env python3
"""TASK-3120 (droppable follow-up): the S0 counter does not start on the pin the 1-wire sensors use.

Context: the OTGW32 has one 1-wire pad (GPIO 4, 3k3 pull-up) and a DI pad (GPIO 5). A user can set the S0 pin to GPIO 4
by hand. With both features enabled on one pin, oneWire.begin(4) and attachInterrupt(4) would both run and neither
works. Rule: while the sensors are enabled on that pin the S0 counter is not started and says why; the Dallas sensors
are the older feature and keep the pin. With the sensors off, S0 may use GPIO 4.

The real initS0Count() is cut out of s0PulseCount.ino and compiled with stubs (pinMode, attachInterrupt, Debug).
S0_INO points at another copy for old-vs-fix. Without a host C++ compiler the compile cases skip.

Run: python tests/test_s0_sensors_pin_collision_3120.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

S0_INO = Path(os.environ.get("OTGW_S0_INO", REPO / "src" / "OTGW-firmware" / "s0PulseCount.ino"))

PRELUDE = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#define PSTR(x) x
#define F(x) x
#define DebugTln(x) std::printf("%s\n", x)
#define DebugTf(...) std::printf(__VA_ARGS__)
#define INPUT_PULLUP 2
#define FALLING 2
struct S0S { bool bEnabled; int8_t iPin; } ;
struct SenS { bool bEnabled; int8_t iPin; };
struct DbgS { bool bSensors; };
struct SetS { S0S s0; SenS sensors; } settings;
struct StS { DbgS debug; } state;
static int g_attached = 0, g_pinmode = 0;
static int g_reserved = -99;
static bool isBoardReservedPin(int pin) { return pin == g_reserved; }
static void pinMode(int, int) { g_pinmode++; }
static void attachInterrupt(int, void (*)(), int) { g_attached++; }
static int digitalPinToInterrupt(int p) { return p; }
static void IRQcounter() {}
static int OTGWs0pulseCount = 0;
"""

MAIN = r"""
int main() {
  struct C { bool s0, sen; int s0pin, senpin, reserved; int expectAttached; } cases[] = {
    {true,  true,  4, 4, -99, 0},   // both on GPIO 4: S0 does not start
    {true,  false, 4, 4, -99, 1},   // sensors off: S0 may use GPIO 4
    {true,  true,  5, 4, -99, 1},   // different pins: both fine
    {false, true,  4, 4, -99, 0},   // S0 disabled: nothing
    {true,  true, -1, -1, -99, 0},  // both not set: S0 not started (not set)
    {true,  true,  5, 5, 5, 0},     // board pin: still refused
  };
  int bad = 0;
  for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    settings.s0.bEnabled = cases[i].s0; settings.s0.iPin = (int8_t)cases[i].s0pin;
    settings.sensors.bEnabled = cases[i].sen; settings.sensors.iPin = (int8_t)cases[i].senpin;
    g_reserved = cases[i].reserved; g_attached = 0; g_pinmode = 0;
    initS0Count();
    if (g_attached != cases[i].expectAttached) { std::printf("FAIL case %u: attached %d expected %d\n", i, g_attached, cases[i].expectAttached); bad++; }
  }
  return bad;
}
"""


def cut(src: str) -> str:
    a = src.index("void initS0Count()")
    b = src.index("} //end SETUP", a) + len("} //end SETUP")
    return src[a:b]


class S0SensorsCollision(unittest.TestCase):
    def test_s0_does_not_start_on_the_sensors_pin(self):
        body = cut(S0_INO.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as td:
            cpp = Path(td) / "s0.cpp"
            cpp.write_text(PRELUDE + body + MAIN, encoding="utf-8")
            exe = compile_cpp(cpp, Path(td) / "s0.exe")
            if exe is None:
                self.skipTest("no host C++ compiler")
            r = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout)

    def test_message_names_the_sensors(self):
        self.assertIn("used by the 1-wire sensors", S0_INO.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
