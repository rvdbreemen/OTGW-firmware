"""TASK-2901: the remote telemetry recorder's NVS secret survives a reboot.

Bench .63, 2026-11-19 (poc.13): every boot after the first enrollment got 409 "uid enrolled with another secret"
and a 24 h backoff. Preferences::getString(key, buf, maxLen) returns nvs_get_str's length INCLUDING the NUL
(framework-arduinoespressif32 libraries/Preferences/src/Preferences.cpp:476-495), so remoteTelemetryIdentity()'s check
against sizeof(secret) - 1 failed on every boot and a new secret replaced the enrolled one.

The test compiles the REAL remoteTelemetryIdentity() (cut from src/OTGW-firmware/RemoteTelemetryDevice.h) against a Preferences
stub with the same return convention, runs it for two boots and checks the second boot keeps the secret.
"""
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import _vcvars  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
DEVICE_H = ROOT / "src" / "OTGW-firmware" / "RemoteTelemetryDevice.h"

PRELUDE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#define PSTR(s) (s)
#define snprintf_P snprintf
static std::map<std::string, std::string> g_nvs;   // survives a "reboot": only the globals below are reset
static int g_puts = 0;
static uint32_t g_rand = 0x12345678u;
struct Preferences {
  bool begin(const char *, bool) { return true; }
  void end() {}
  // nvs_get_str semantics: the length counts the NUL terminator; 0 on a missing key or a too small buffer.
  size_t getString(const char *key, char *value, size_t maxLen) {
    auto it = g_nvs.find(key);
    if (it == g_nvs.end()) return 0;
    size_t len = it->second.size() + 1;
    if (len > maxLen) return 0;
    memcpy(value, it->second.c_str(), len);
    return len;
  }
  size_t putString(const char *key, const char *value) { g_nvs[key] = value; g_puts++; return strlen(value); }
};
static void platformGetMacAddress(uint8_t *mac) { const uint8_t m[6] = {0xac, 0x27, 0x6e, 0xce, 0x45, 0xd8}; memcpy(mac, m, 6); }
static uint32_t platformHardwareRandom() { g_rand = g_rand * 1664525u + 1013904223u; return g_rand; }
static char remoteTelemetryUid[18] = "";
static char remoteTelemetrySecret[33] = "";
"""

MAIN = r"""
int main() {
  remoteTelemetryIdentity();                                   // first boot: no secret yet, one is made and stored
  std::string first = remoteTelemetrySecret;
  memset(remoteTelemetrySecret, 0, sizeof(remoteTelemetrySecret));    // reboot: RAM is gone, NVS stays
  remoteTelemetryIdentity();
  printf("len %zu same %d puts %d uid %s\n", first.size(), first == remoteTelemetrySecret ? 1 : 0, g_puts, remoteTelemetryUid);
  return 0;
}
"""


def identity_source(text):
    start = text.index("static void remoteTelemetryIdentity() {")
    return text[start:text.index("\n}\n", start) + 3]


def run(device_text):
    """Two boots of the given RemoteTelemetryDevice.h's remoteTelemetryIdentity(); returns the printed fields, None
    without a host compiler (MSVC on Windows, else g++ or clang++)."""
    bat = _vcvars()
    gxx = None if bat else (shutil.which("g++") or shutil.which("clang++"))
    if not bat and not gxx:
        return None
    work = Path(tempfile.mkdtemp(prefix="crid_"))
    try:
        (work / "main.cpp").write_text(PRELUDE + identity_source(device_text) + MAIN, encoding="utf-8")
        exe = work / "t.exe"
        if bat:
            cmd = f'call "{bat}" >nul && cl /nologo /std:c++20 /EHsc /Fe:"{exe}" /Fo:"{work}\\\\" "{work / "main.cpp"}"'
            r = subprocess.run(cmd, shell=True, cwd=work, capture_output=True, text=True)
        else:
            r = subprocess.run([gxx, "-std=c++20", "-o", str(exe), str(work / "main.cpp")], cwd=work,
                               capture_output=True, text=True)
        if r.returncode:
            raise AssertionError("host compile failed:\n" + r.stdout[-3000:] + r.stderr[-1000:])
        out = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60).stdout.split()
        return dict(zip(out[0::2], out[1::2]))
    finally:
        shutil.rmtree(work, ignore_errors=True)


class SecretSurvivesReboot(unittest.TestCase):
    def test_second_boot_keeps_the_enrolled_secret(self):
        r = run(DEVICE_H.read_text(encoding="utf-8"))
        if r is None:
            self.skipTest("no host C++ compiler")
        self.assertEqual(r["len"], "32")
        self.assertEqual(r["uid"], "otgw-ac276ece45d8")
        self.assertEqual(r["same"], "1", r)
        self.assertEqual(r["puts"], "1", r)      # made and stored once, never again


if __name__ == "__main__":
    # python tests/test_remote_telemetry_identity_secret.py <RemoteTelemetryDevice.h>: run another copy (red run on the old code).
    if len(sys.argv) > 1:
        print(run(Path(sys.argv[1]).read_text(encoding="utf-8")))
    else:
        unittest.main()
