"""TASK-2921: the prerelease bench key 'B' makes the REST gate act as on a starved heap on a healthy heap.

The real restEffectiveInflightCap(), sendApiError() and sendApiBusy() plus the real TASK-2921 block (flag,
toggle, restForceStarved()) are cut out of restAPI.ino and compiled twice: with -D_VERSION_PRERELEASE (the key
exists) and without it (production: the check is a constant false). Stubs as in test_rest_busy_starved_heap.py.
A second check reads handleDebug.ino: the 'B' case and its help line sit inside a _VERSION_PRERELEASE guard.
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

REST_INO = Path(os.environ.get("OTGW_RESTAPI_INO", REPO / "src" / "OTGW-firmware" / "restAPI.ino"))
DEBUG_INO = REPO / "src" / "OTGW-firmware" / "handleDebug.ino"

STUBS = r"""
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
typedef const char* PGM_P;
#define snprintf_P snprintf
#define REST_MAX_INFLIGHT 2
struct { struct { uint8_t iRestCap = 2; } debug; } state;   // TASK-3091: restCapBase() reads state.debug.iRestCap in a prerelease build
static uint32_t g_maxBlock = 40000;
static uint32_t platformMaxFreeBlock() { return g_maxBlock; }
static int restResponseStatus = 0;
static void webPushHeader(const __FlashStringHelper* n, const __FlashStringHelper* v) {
  printf("HEADER %s: %s\n", reinterpret_cast<const char*>(n), reinterpret_cast<const char*>(v)); }
static void sendCorsOriginHeader() {}
static void webSend(int code, const __FlashStringHelper*, const char* body) { printf("SENT %d %s\n", code, body); }
static void webAbortCurrentRequest() { printf("CLOSED without a response\n"); }
"""

MAIN = r"""
#if !defined(_VERSION_PRERELEASE)
static bool restDebugToggleForceStarved() { return false; }   /* production has no toggle */
#endif
int main(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "B") == 0) printf("TOGGLE %s\n", restDebugToggleForceStarved() ? "ON" : "off");
  printf("CAP %u\n", (unsigned)restEffectiveInflightCap());
  sendApiBusy(F("Server busy: too many concurrent requests, please retry"));
  return 0;
}
"""


def cut(src, signature):
    m = re.search(r"^" + re.escape(signature) + r".*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError(signature + " not found")
    return m.group(0)


def cap_base_block(src):
    """TASK-3091: restCapBase(), the prerelease runtime override of the in-flight cap, with its release twin."""
    m = re.search(r"^#if defined\(_VERSION_PRERELEASE\)\nstatic inline uint8_t restCapBase\(\).*?^#endif\n", src, re.S | re.M)
    if not m:
        raise AssertionError("restCapBase() block (TASK-3091) not found in restAPI.ino")
    return m.group(0)


def task_block(src):
    m = re.search(r"^#if defined\(_VERSION_PRERELEASE\)\nstatic std::atomic<bool> s_restForceStarved.*?^#endif\n", src, re.S | re.M)
    if not m:
        raise AssertionError("TASK-2921 block (s_restForceStarved) not found in restAPI.ino")
    return m.group(0)


class ForceStarvedKey(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = REST_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        consts = "".join(m.group(0) + "\n" for m in re.finditer(r"^static const uint32_t REST_BUSY_\w+\s*=.*;", src, re.M))
        body = (task_block(src) + cap_base_block(src) + cut(src, "static inline uint8_t restEffectiveInflightCap(") + "\n"
                + cut(src, "static void sendApiError(") + "\n" + consts + cut(src, "static void sendApiBusy(") + "\n")
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = {}
        for name, define in (("pre", "#define _VERSION_PRERELEASE \"beta.1\"\n"), ("prod", "")):
            cpp = Path(cls.tmp.name) / (name + ".cpp")
            cpp.write_text(define + STUBS + body + MAIN, encoding="utf-8")
            exe = compile_cpp(cpp, Path(cls.tmp.name) / (name + (".exe" if os.name == "nt" else "")))
            if exe is None:
                raise unittest.SkipTest("no host C++ compiler")
            cls.exe[name] = exe

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_exe(self, name, *args):
        r = subprocess.run([str(self.exe[name]), *args], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r.stdout

    def test_key_on_takes_the_starved_branch_on_a_healthy_heap(self):
        out = self.run_exe("pre", "B")
        self.assertIn("TOGGLE ON", out)
        self.assertIn("CAP 1", out)
        self.assertIn("CLOSED without a response", out)
        self.assertNotIn("SENT 503", out)
        self.assertNotIn("HEADER", out)

    def test_key_off_keeps_the_503_path_and_the_full_cap(self):
        out = self.run_exe("pre")
        self.assertIn("CAP 2", out)
        self.assertIn("HEADER Retry-After: 1", out)
        self.assertIn("SENT 503", out)
        self.assertNotIn("CLOSED", out)

    def test_production_build_has_no_key(self):
        out = self.run_exe("prod", "B")
        self.assertIn("TOGGLE off", out)
        self.assertIn("CAP 2", out)
        self.assertIn("SENT 503", out)
        self.assertNotIn("CLOSED", out)

    def test_the_B_key_and_help_line_are_prerelease_only(self):
        dbg = DEBUG_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        for needle in ("case 'B':", 'B) bench: REST gate'):
            i = dbg.index(needle)
            before = dbg[:i]
            self.assertGreater(before.rfind("#if defined(_VERSION_PRERELEASE)"), before.rfind("#endif"), needle)


if __name__ == "__main__":
    unittest.main()
