"""The REST busy-503 path must not allocate a response on a starved heap (.61 panic, beta.388, 2026-11-19).

Decoded panic (task async_tcp): processAPI (restAPI.ino, the in-flight gate) -> sendApiBusy -> sendApiError
-> webSend -> webApplyHeaders -> AsyncWebServerResponse::addHeader -> operator new fails ->
__cxa_allocate_exception fails -> std::terminate -> abort. The gate that protects the heap built its own 503
with a Retry-After header.

The test cuts the real sendApiError() and sendApiBusy() out of restAPI.ino and compiles them against stubs:
webSend() stands for building and sending a response and "aborts" (exit 134, like the device's abort()) when
the largest free block is too small for the response objects, as operator new does there. Nothing of the two
functions is re-implemented. OTGW_RESTAPI_INO=<copy of an older restAPI.ino> runs it against other code.
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

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
typedef const char* PGM_P;
#define snprintf_P snprintf
static uint32_t g_maxBlock = 40000;
static const uint32_t RESPONSE_NEEDS = 1024;   // response object + header list nodes + content, rounded up
static uint32_t platformMaxFreeBlock() { return g_maxBlock; }
static int restResponseStatus = 0;
static int g_headers = 0;
static void webPushHeader(const __FlashStringHelper* n, const __FlashStringHelper* v) {
  printf("HEADER %s: %s\n", reinterpret_cast<const char*>(n), reinterpret_cast<const char*>(v)); g_headers++; }
static void sendCorsOriginHeader() {}
static void webSend(int code, const __FlashStringHelper*, const char* body) {
  if (g_maxBlock < RESPONSE_NEEDS) { printf("ABORT operator new in addHeader\n"); fflush(stdout); exit(134); }
  printf("SENT %d %s\n", code, body);
}
static void webAbortCurrentRequest() { printf("CLOSED without a response\n"); }
static inline bool restForceStarved() { return false; }   // TASK-2921 bench key: off (its own test covers it)
"""

MAIN = r"""
int main(int argc, char** argv) {
  g_maxBlock = (uint32_t)atoi(argv[1]);
  sendApiBusy(F("Server busy: too many concurrent requests, please retry"));
  printf("RETURNED status=%d headers=%d\n", restResponseStatus, g_headers);
  return 0;
}
"""


def cut(src, signature):
    m = re.search(r"^" + re.escape(signature) + r".*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError(signature + " not found in " + str(REST_INO))
    return m.group(0)


class BusyOnStarvedHeap(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = REST_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        body = cut(src, "static void sendApiError(") + "\n" + cut(src, "static void sendApiBusy(") + "\n"
        consts = "".join(m.group(0) + "\n" for m in re.finditer(r"^static const uint32_t REST_BUSY_\w+\s*=.*;", src, re.M))
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + consts + body + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t" + (".exe" if os.name == "nt" else "")))
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_at(self, max_block):
        r = subprocess.run([str(self.exe), str(max_block)], capture_output=True, text=True)
        return r.returncode, r.stdout

    def test_starved_heap_closes_without_building_a_response(self):
        rc, out = self.run_at(300)   # the .61 panic: a few hundred bytes left in the largest block
        self.assertNotIn("ABORT", out, "the busy path allocated a response on a starved heap: " + out)
        self.assertEqual(rc, 0, out)
        self.assertIn("CLOSED without a response", out)
        self.assertNotIn("HEADER", out, "a header was queued although no response is sent: " + out)

    def test_healthy_heap_keeps_the_503_with_retry_after(self):
        rc, out = self.run_at(40000)
        self.assertEqual(rc, 0, out)
        self.assertIn("HEADER Retry-After: 1", out)
        self.assertIn("SENT 503", out)
        self.assertNotIn("CLOSED", out)


if __name__ == "__main__":
    unittest.main()
