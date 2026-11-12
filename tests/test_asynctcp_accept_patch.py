#!/usr/bin/env python3
"""TASK-1746.1 / ADR-232: host test for the AsyncTCP 7befe25 build-time patch.

The code under test is the real AsyncTCP_detail::tcp_accept(), cut out of AsyncTCP.cpp and compiled
with a host C++ compiler. Stubs replace lwIP (tcp_abort counts its calls), the server, the event queue
and the allocations: the event packet and the AsyncClient each have a class operator new that can be
told to fail. Checked on every path (the lwIP contract, not a code shape):
  - a path that calls tcp_abort() returns ERR_ABRT and calls it at most once;
  - no AsyncClient is left alive on a failure path;
  - the normal path returns ERR_OK, queues one event and does not abort.

Which source:
  OTGW_ASYNCTCP_CPP set   -> that file as it is (red on plain v3.4.10, green on the patched file).
  not set                 -> .pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.cpp; if that is still plain
                             3.4.10, the stored patch is applied in memory first, so the test checks what
                             the build will compile.
SelfCheck rebuilds plain 3.4.10 by applying the patch in reverse and asserts it goes red, so the test
proves on every run that it detects the bug. HashGate checks scripts/asynctcp_patch.ensure_patched().
Without a compiler or without the libdeps copy the compile cases are skipped, they do not pass silently.

Run: python tests/test_asynctcp_accept_patch.py
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
sys.path.insert(0, str(REPO / "scripts"))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import asynctcp_patch as ap  # noqa: E402

LIBDEPS = REPO / ".pio" / "libdeps" / "esp32-combo" / "AsyncTCP" / "src" / "AsyncTCP.cpp"
ENV_SRC = os.environ.get("OTGW_ASYNCTCP_CPP")

HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
typedef int8_t err_t;
#define ERR_OK 0
#define ERR_ABRT -13
#define async_tcp_log_e(...) ((void)0)
struct tcp_pcb { int id; };
static int g_aborts = 0;
static void tcp_abort(tcp_pcb *) { g_aborts++; }
static bool g_fail_event = false, g_fail_client = false;
static int g_clients_alive = 0, g_queued = 0;
enum { LWIP_TCP_ACCEPT = 1, LWIP_TCP_RECV = 2 };
struct AsyncServer;
struct AsyncClient {
  tcp_pcb *_pcb;
  explicit AsyncClient(tcp_pcb *p) : _pcb(p) { g_clients_alive++; }
  ~AsyncClient() { g_clients_alive--; }
  tcp_pcb *pcb() { return _pcb; }
  void setNoDelay(bool) {}
  static void *operator new(size_t n, const std::nothrow_t &) noexcept { return g_fail_client ? nullptr : std::malloc(n); }
  static void operator delete(void *p) noexcept { std::free(p); }
};
struct lwip_tcp_event_packet_t {
  int event;
  AsyncClient *client;
  struct { AsyncServer *server; } accept;
  struct { void *pb; } recv;
  static void *operator new(size_t n, const std::nothrow_t &) noexcept { return g_fail_event ? nullptr : std::malloc(n); }
  static void operator delete(void *p) noexcept { std::free(p); }
};
struct AsyncServer { void *_connect_cb; bool _noDelay; };
struct queue_mutex_guard {};
static lwip_tcp_event_packet_t *g_last = nullptr;
static void _prepend_async_event(lwip_tcp_event_packet_t *e) { g_queued++; g_last = e; }
static void _free_event(lwip_tcp_event_packet_t *e) { delete e; }
struct AsyncTCP_detail { static int8_t tcp_accept(void *arg, tcp_pcb *pcb, int8_t err); };

/*TCP_ACCEPT*/

static int fails = 0;
static void check(bool ok, const char *what) { if (!ok) { std::printf("FAIL %s\n", what); fails++; } }
static void run(const char *name, bool cb, bool fail_event, bool fail_client, bool expect_ok) {
  g_aborts = 0; g_queued = 0; g_clients_alive = 0; g_last = nullptr;
  g_fail_event = fail_event; g_fail_client = fail_client;
  tcp_pcb pcb{1};
  AsyncServer srv{cb ? (void *)1 : nullptr, true};
  int8_t r = AsyncTCP_detail::tcp_accept(&srv, &pcb, 0);
  char buf[160];
  if (expect_ok) {
    std::snprintf(buf, sizeof buf, "%s: returns ERR_OK, queues 1, no abort (r=%d q=%d a=%d)", name, r, g_queued, g_aborts);
    check(r == ERR_OK && g_queued == 1 && g_aborts == 0, buf);
    if (g_last) { delete g_last->client; delete g_last; }
    return;
  }
  std::snprintf(buf, sizeof buf, "%s: tcp_abort at most once (a=%d)", name, g_aborts);
  check(g_aborts <= 1, buf);
  std::snprintf(buf, sizeof buf, "%s: aborted pcb reported as ERR_ABRT (r=%d a=%d)", name, r, g_aborts);
  check(g_aborts == 0 || r == ERR_ABRT, buf);
  std::snprintf(buf, sizeof buf, "%s: nothing queued (q=%d)", name, g_queued);
  check(g_queued == 0, buf);
  std::snprintf(buf, sizeof buf, "%s: no AsyncClient left alive (alive=%d)", name, g_clients_alive);
  check(g_clients_alive == 0, buf);
}
int main() {
  run("normal accept", true, false, false, true);
  run("no onConnect callback", false, false, false, false);
  run("event allocation fails", true, true, false, false);
  run("client allocation fails", true, false, true, false);
  int8_t r = AsyncTCP_detail::tcp_accept(nullptr, nullptr, 0);
  check(r == ERR_ABRT, "null pcb: ERR_ABRT");
  std::printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
  return fails ? 1 : 0;
}
"""


def slice_accept(src):
    src = src.replace("\r\n", "\n")
    m = re.search(r"^int8_t AsyncTCP_detail::tcp_accept\(void \*arg, tcp_pcb \*pcb, int8_t err\)\s*\{", src, re.M)
    if not m:
        raise AssertionError("tcp_accept() not found in AsyncTCP.cpp")
    i, depth = m.end() - 1, 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    raise AssertionError("unbalanced braces in tcp_accept()")


def reverse_patch(text):
    """Plain 3.4.10 from the patched text: the stored hunks applied backwards."""
    for old, new in ap._hunks(ap.PATCH_FILE.read_text(encoding="utf-8")):
        assert text.count(new) == 1, "patched block not found once"
        text = text.replace(new, old)
    return text


def run_harness(cpp_text):
    """Compile the harness around tcp_accept() from cpp_text. Returns (returncode, output) or None."""
    with tempfile.TemporaryDirectory() as d:
        src = Path(d) / "t.cpp"
        src.write_text(HARNESS.replace("/*TCP_ACCEPT*/", slice_accept(cpp_text)), encoding="utf-8")
        exe = compile_cpp(src, Path(d) / ("t.exe" if os.name == "nt" else "t"))
        if exe is None:
            return None
        r = subprocess.run([str(exe)], capture_output=True, text=True)
        return r.returncode, r.stdout


def source_under_test():
    if ENV_SRC:
        return Path(ENV_SRC).read_text(encoding="utf-8").replace("\r\n", "\n"), f"{ENV_SRC} (as is)"
    if not LIBDEPS.is_file():
        return None, None
    text = LIBDEPS.read_text(encoding="utf-8").replace("\r\n", "\n")
    h = ap._sha(text)
    if h == ap.BASE_SHA256:
        return ap.apply_patch(text, ap.PATCH_FILE.read_text(encoding="utf-8")), "libdeps 3.4.10 + patch in memory"
    if h == ap.PATCHED_SHA256:
        return text, "libdeps, already patched"
    raise AssertionError(f"libdeps AsyncTCP.cpp is neither 3.4.10 nor patched (sha256 {h[:12]})")


class Contract(unittest.TestCase):
    def test_tcp_accept_contract(self):
        text, label = source_under_test()
        if text is None:
            self.skipTest("no libdeps AsyncTCP copy (needs one esp32-combo build) and OTGW_ASYNCTCP_CPP not set")
        res = run_harness(text)
        if res is None:
            self.skipTest("no host C++ compiler")
        rc, out = res
        print(f"[{label}]\n{out}")
        self.assertEqual(rc, 0, f"tcp_accept() breaks the lwIP contract on {label}:\n{out}")


class SelfCheck(unittest.TestCase):
    def test_plain_3410_goes_red(self):
        text, _ = source_under_test()
        if text is None or ENV_SRC:
            self.skipTest("needs the libdeps copy and no OTGW_ASYNCTCP_CPP")
        base = reverse_patch(text)
        self.assertEqual(ap._sha(base), ap.BASE_SHA256, "reverse patch does not give plain 3.4.10")
        res = run_harness(base)
        if res is None:
            self.skipTest("no host C++ compiler")
        rc, out = res
        self.assertNotEqual(rc, 0, "plain 3.4.10 passes, so the test does not detect the bug")
        self.assertIn("aborted pcb reported as ERR_ABRT", out)


class HashGate(unittest.TestCase):
    def setUp(self):
        text, _ = source_under_test() if not ENV_SRC else (None, None)
        if text is None:
            self.skipTest("needs the libdeps copy")
        self.patched = text
        self.base = reverse_patch(text)
        self.d = tempfile.TemporaryDirectory()
        self.f = Path(self.d.name) / "AsyncTCP.cpp"

    def tearDown(self):
        self.d.cleanup()

    def test_base_is_patched_then_left_alone(self):
        self.f.write_bytes(self.base.encode("utf-8"))
        self.assertEqual(ap.ensure_patched(self.f), "applied")
        self.assertEqual(ap._sha(self.f.read_text(encoding="utf-8")), ap.PATCHED_SHA256)
        self.assertEqual(ap.ensure_patched(self.f), "already")

    def test_crlf_copy_keeps_crlf(self):
        self.f.write_bytes(self.base.replace("\n", "\r\n").encode("utf-8"))
        self.assertEqual(ap.ensure_patched(self.f), "applied")
        raw = self.f.read_bytes()
        self.assertIn(b"\r\n", raw)
        self.assertEqual(ap._sha(raw.decode("utf-8").replace("\r\n", "\n")), ap.PATCHED_SHA256)

    def test_unknown_file_stops_the_build(self):
        self.f.write_bytes((self.base + "// moved upstream\n").encode("utf-8"))
        with self.assertRaises(ap.PatchError):
            ap.ensure_patched(self.f)

    def test_missing_file_stops_the_build(self):
        with self.assertRaises(ap.PatchError):
            ap.ensure_patched(Path(self.d.name) / "missing.cpp")


if __name__ == "__main__":
    unittest.main(verbosity=2)
