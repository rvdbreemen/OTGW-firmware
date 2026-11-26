#!/usr/bin/env python3
"""TASK-2949.3 / ADR-266: the MQTT socket honours EMC_TCP_NODELAY, and the firmware builds it with Nagle on.

The vendored espMqttClient (src/libraries/espMqttClient, a byte copy of the fork tag in OTGW-PROVENANCE.md) gives the sync transport
an EMC_TCP_NODELAY option: 1 = TCP_NODELAY on (upstream's behaviour), 0 = Nagle on. ADR-266 (Accepted 25 Nov 2026) switches the firmware to 0 so a burst of
small SAT packets leaves as 2-3 segments instead of 16. platformio.ini carries -DEMC_TCP_NODELAY=0.

The real src/Transport/ClientSync.cpp is compiled (ARDUINO_ARCH_ESP32 path) against small stubs of the Arduino network classes; the
stub records every setSocketOption() call. Both connect() overloads must set TCP_NODELAY to EMC_TCP_NODELAY.

OTGW_ESPMQTT_SRC=<dir> compiles another copy of the library src (old-vs-fix: the copy on origin/dev before TASK-2949.3 sets the option to 1 whatever the flag says).
Run: python tests/test_espmqttclient_nodelay.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import _vcvars  # noqa: E402  (finds the MSVC environment when there is no g++/clang++)

REPO = HERE.parent
LIB_SRC = Path(os.environ.get("OTGW_ESPMQTT_SRC", REPO / "src" / "libraries" / "espMqttClient" / "src"))

STUBS = {
    "Network.h": r"""
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <IPAddress.h>
struct NetworkClient {
  int connect(IPAddress, uint16_t, uint32_t) { return 1; }
  int connect(const char*, uint16_t, uint32_t) { return 1; }
  void setNoDelay(bool) {}
  int setSocketOption(int level, int optname, const void* val, size_t len);
  int fd() { return 3; }
  size_t write(const uint8_t*, size_t n) { return n; }
  int read(uint8_t*, size_t) { return 0; }
  void stop() {}
  bool connected() { return true; }
};
""",
    "IPAddress.h": r"""
#pragma once
#include <stdint.h>
struct IPAddress { uint32_t a = 0; IPAddress() {} IPAddress(uint32_t v) : a(v) {} };
""",
    "lwip/sockets.h": r"""
#pragma once
#include <stddef.h>
#define IPPROTO_TCP 6
#define TCP_NODELAY 1
#define MSG_DONTWAIT 0x08
typedef long ssize_t;
inline ssize_t send(int, const void*, size_t n, int) { return (ssize_t)n; }
""",
}

MAIN = r"""
#include <cstdio>
#include "Transport/ClientSync.cpp"
static int g_calls = 0, g_last = -99, g_level = 0, g_opt = 0;
int NetworkClient::setSocketOption(int level, int optname, const void* val, size_t len) {
  g_calls++; g_level = level; g_opt = optname; g_last = (len == sizeof(int)) ? *(const int*)val : -1; return 0;
}
int main() {
  espMqttClientInternals::ClientSync c;
  c.connect(IPAddress(1), 1883);
  int a = g_calls, va = g_last;
  c.connect("broker.local", 1883);
  std::printf("%d %d %d %d %d %d\n", a, va, g_calls, g_last, g_level, g_opt);
  return 0;
}
"""


def build(workdir: Path, flag: str):
    for rel, text in STUBS.items():
        p = workdir / "stubs" / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
    cpp = workdir / f"nodelay_{flag or 'default'}.cpp"
    cpp.write_text(MAIN, encoding="utf-8")
    exe = workdir / f"nodelay_{flag or 'default'}.exe"
    defs = ["-DARDUINO", "-DARDUINO_ARCH_ESP32"] + ([f"-DEMC_TCP_NODELAY={flag}"] if flag != "" else [])
    inc = [str(workdir / "stubs"), str(LIB_SRC)]
    for cc in ("g++", "clang++"):
        if shutil.which(cc):
            subprocess.run([cc, "-std=c++17", *defs, *[f"-I{i}" for i in inc], "-o", str(exe), str(cpp)], check=True)
            return exe
    bat = _vcvars()
    if not bat:
        return None
    cmd = (f'call "{bat}" >nul && cl /nologo /std:c++17 /EHsc {" ".join("/D" + d[2:] for d in defs)} '
           f'{" ".join("/I" + chr(34) + i + chr(34) for i in inc)} /Fe:"{exe}" /Fo:"{workdir}\\\\" "{cpp}"')
    r = subprocess.run(cmd, shell=True, cwd=workdir, capture_output=True, text=True)
    if r.returncode:
        raise AssertionError("host compile failed:\n" + r.stdout[-3000:] + r.stderr[-1000:])
    return exe


class NoDelayOption(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.d = Path(tempfile.mkdtemp(prefix="emc_nodelay_"))
        cls.exe = {f: build(cls.d, f) for f in ("0", "1", "")}

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.d, ignore_errors=True)

    def run_exe(self, flag):
        if self.exe[flag] is None:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe[flag])], capture_output=True, text=True, check=True).stdout.split()
        return [int(x) for x in out]

    def test_flag_0_leaves_nagle_on_for_both_connect_overloads(self):
        calls1, val1, calls2, val2, level, opt = self.run_exe("0")
        self.assertEqual((calls1, calls2), (1, 2), "one setSocketOption per connect()")
        self.assertEqual((level, opt), (6, 1), "IPPROTO_TCP / TCP_NODELAY")
        self.assertEqual((val1, val2), (0, 0), "EMC_TCP_NODELAY=0: TCP_NODELAY is switched off, Nagle stays on")

    def test_flag_1_keeps_no_delay_as_upstream(self):
        _, val1, _, val2, _, _ = self.run_exe("1")
        self.assertEqual((val1, val2), (1, 1))

    def test_default_is_no_delay_so_the_library_alone_changes_nothing(self):
        _, val1, _, val2, _, _ = self.run_exe("")
        self.assertEqual((val1, val2), (1, 1))


class FirmwareBuildFlag(unittest.TestCase):
    def test_platformio_builds_the_mqtt_socket_with_nagle_on(self):
        ini = (REPO / "platformio.ini").read_text(encoding="utf-8")
        self.assertRegex(ini, r"(?m)^\s*-DEMC_TCP_NODELAY=0\s*$", "ADR-266: the firmware builds espMqttClient with Nagle on")
        self.assertNotRegex(ini, r"(?m)^\s*-DEMC_TCP_NODELAY=1")

    def test_flag_is_in_the_shared_build_flags_next_to_the_connect_timeout(self):
        ini = (REPO / "platformio.ini").read_text(encoding="utf-8")
        a = ini.index("-DEMC_CONNECT_TIMEOUT_MS")
        b = ini.index("-DEMC_TCP_NODELAY=0")
        self.assertLess(abs(a - b), 900, "the two EMC_ flags sit together")


if __name__ == "__main__":
    unittest.main(verbosity=2)
