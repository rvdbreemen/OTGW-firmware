#!/usr/bin/env python3
"""TASK-1560 / ADR-210: GET /api/v2/device/coredump streams the stored core dump.

Two layers:
1. Source checks on restAPI.ino: the route is wired in handleDevice(), the handler calls
   checkHttpAuth() before producing any byte (same function as /safeboot and /debug/flags),
   answers 404 without a dump, streams through a filler, and never allocates the dump size.
   Application code must not touch esp_core_dump / esp_partition directly.
2. platformCoreDumpSize() and platformCoreDumpRead() are cut out of the real
   src/libraries/Platform/src/platform_esp32.h and compiled on the host against a fake flash
   partition: chunked reads reassemble the image exactly, reads clamp at the image length (not
   the partition), a read past the end gives 0, no valid dump gives size 0.
   platformCoreDumpAppSha() returns the dump's own app ELF SHA256 (lowercase hex, bounded by the
   caller's buffer, false without a valid dump or without hex digits).

OTGW_PLATFORM_H and OTGW_REST_INO point at other copies of platform_esp32.h and restAPI.ino for
old-vs-fix runs.
Run: python tests/test_device_coredump_api.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
REST = Path(os.environ.get("OTGW_REST_INO", ROOT / "src" / "OTGW-firmware" / "restAPI.ino"))
PLATFORM = Path(os.environ.get("OTGW_PLATFORM_H", ROOT / "src/libraries/Platform/src/platform_esp32.h"))

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <vector>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
enum { ESP_PARTITION_TYPE_DATA = 1 };
enum { ESP_PARTITION_SUBTYPE_DATA_COREDUMP = 3 };
struct esp_partition_t { uint32_t address; uint32_t size; };
struct esp_core_dump_summary_t { uint8_t app_elf_sha256[65]; };
static char g_sha[80] = "";
static bool g_sumOk = true;
static const uint32_t PART_ADDR = 0x3F0000, PART_SIZE = 0x10000;
static std::vector<uint8_t> g_flash(PART_SIZE, 0xFF);
static esp_partition_t g_part = { PART_ADDR, PART_SIZE };
static bool g_valid = false;
static size_t g_imgSize = 0;
static size_t g_imgOff = 0;   // image start inside the partition
esp_err_t esp_core_dump_image_check() { return g_valid ? ESP_OK : ESP_FAIL; }
esp_err_t esp_core_dump_image_get(size_t *a, size_t *s) { if (!g_valid) return ESP_FAIL; *a = PART_ADDR + g_imgOff; *s = g_imgSize; return ESP_OK; }
esp_err_t esp_core_dump_get_summary(esp_core_dump_summary_t *s) {
  if (!g_valid || !g_sumOk) return ESP_FAIL;
  memset(s->app_elf_sha256, 0, sizeof(s->app_elf_sha256));
  strncpy((char*)s->app_elf_sha256, g_sha, sizeof(s->app_elf_sha256) - 1);
  return ESP_OK;
}
const esp_partition_t* esp_partition_find_first(int t, int st, const char*) { return (t == ESP_PARTITION_TYPE_DATA && st == ESP_PARTITION_SUBTYPE_DATA_COREDUMP) ? &g_part : nullptr; }
esp_err_t esp_partition_read(const esp_partition_t* p, size_t off, void* dst, size_t len) {
  if (off + len > p->size) return ESP_FAIL;
  memcpy(dst, g_flash.data() + off, len); return ESP_OK;
}
"""

MAIN = r"""
int main() {
  // image of 5000 bytes at partition offset 0, byte i = i*7+3
  g_valid = true; g_imgSize = 5000; g_imgOff = 0;
  for (size_t i = 0; i < g_imgSize; i++) g_flash[i] = (uint8_t)(i * 7 + 3);
  printf("size|%zu\n", platformCoreDumpSize());
  // reassemble in 1460-byte chunks as the server would ask
  std::vector<uint8_t> got; uint8_t buf[1460]; size_t idx = 0, n;
  while ((n = platformCoreDumpRead(idx, buf, sizeof(buf))) > 0) { got.insert(got.end(), buf, buf + n); idx += n; }
  bool same = got.size() == g_imgSize;
  for (size_t i = 0; same && i < g_imgSize; i++) same = got[i] == (uint8_t)(i * 7 + 3);
  printf("reassembled|%zu|%d\n", got.size(), (int)same);
  printf("past_end|%zu\n", platformCoreDumpRead(5000, buf, sizeof(buf)));
  printf("last_clamped|%zu\n", platformCoreDumpRead(4990, buf, sizeof(buf)));
  // image starting inside the partition
  g_imgOff = 0x100; g_imgSize = 64;
  for (size_t i = 0; i < 64; i++) g_flash[0x100 + i] = (uint8_t)(0xA0 + i);
  size_t r = platformCoreDumpRead(0, buf, 64);
  printf("offset_image|%zu|%d\n", r, (int)(r == 64 && buf[0] == 0xA0 && buf[63] == 0xA0 + 63));
  // image claims to run past the partition: refused
  g_imgOff = 0xFF00; g_imgSize = 0x400;
  printf("overrun|%zu\n", platformCoreDumpRead(0, buf, 16));
  // no valid dump
  g_valid = false;
  printf("none|%zu|%zu\n", platformCoreDumpSize(), platformCoreDumpRead(0, buf, 16));
  // app ELF sha: the dump's own, not the running build's
  char sha[65];
  g_valid = true;
  strcpy(g_sha, "0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef");
  bool ok = platformCoreDumpAppSha(sha, sizeof(sha));
  printf("sha_full|%d|%s\n", (int)ok, sha);
  strcpy(g_sha, "a1b2c3d4e");   // the IDF default keeps 9 characters
  ok = platformCoreDumpAppSha(sha, sizeof(sha));
  printf("sha_9|%d|%s\n", (int)ok, sha);
  char small[5];
  ok = platformCoreDumpAppSha(small, sizeof(small));
  printf("sha_small|%d|%s\n", (int)ok, small);
  strcpy(g_sha, "");
  ok = platformCoreDumpAppSha(sha, sizeof(sha));
  printf("sha_empty|%d|%s\n", (int)ok, sha);
  strcpy(g_sha, "zz");
  ok = platformCoreDumpAppSha(sha, sizeof(sha));
  printf("sha_nonhex|%d|%s\n", (int)ok, sha);
  strcpy(g_sha, "a1b2c3d4e");
  g_sumOk = false;
  ok = platformCoreDumpAppSha(sha, sizeof(sha));
  printf("sha_nosummary|%d|%s\n", (int)ok, sha);
  g_sumOk = true; g_valid = false;
  ok = platformCoreDumpAppSha(sha, sizeof(sha));
  printf("sha_nodump|%d|%s\n", (int)ok, sha);
  return 0;
}
"""


def cut(text, start_marker, end_marker):
    a = text.index(start_marker)
    b = text.index(end_marker, a)
    return text[a:b]


class CoreDumpSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rest = REST.read_text(encoding="utf-8")
        cls.handler = cut(cls.rest, "void sendDeviceCoreDump()", "\n}\n") + "\n}\n"

    def test_route_in_handle_device(self):
        dev = cut(self.rest, "static void handleDevice(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {", "static void handleFlash(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {")
        self.assertRegex(dev, r'PSTR\("coredump"\)\) == 0\)\s*\{\s*sendDeviceCoreDump\(\)')
        self.assertIn("HTTP_GET", dev)  # GET only: the method gate sits before the branches

    def test_admin_gate_before_any_output(self):
        h = self.handler
        self.assertIn("checkHttpAuth()", h)
        self.assertLess(h.index("checkHttpAuth()"), h.index("platformCoreDumpSize()"))
        self.assertLess(h.index("checkHttpAuth()"), h.index("beginResponse"))
        # the same function guards /debug/flags and the other admin routes
        self.assertIn("if (!checkHttpAuth()) return;", self.rest)

    def test_404_when_no_dump(self):
        self.assertRegex(self.handler, r"dumpSize == 0\)\s*\{\s*sendApiError\(404")

    def test_streams_through_shim_filler_with_attachment_headers(self):
        h = self.handler
        self.assertIn("platformCoreDumpRead(index, buf, maxLen)", h)
        self.assertIn("application/octet-stream", h)
        self.assertIn("Content-Disposition", h)

    def test_attachment_named_after_the_dumps_own_app_sha(self):
        h = self.handler
        # the stored dump is usually from an older build: the running githash would name the wrong .elf
        self.assertNotIn("_VERSION_GITHASH", h)
        self.assertIn("platformCoreDumpAppSha(appSha, sizeof(appSha))", h)
        self.assertIn("otgw-coredump-app-%.9s.bin", h)
        self.assertIn("otgw-coredump.bin", h)   # fallback without a readable summary
        self.assertRegex(h, r'if \(haveSha\) webPushHeader\(F\("X-Coredump-App-Sha256"\), appSha\)')

    def test_never_allocates_the_dump(self):
        h = self.handler
        for bad in ("malloc", "new ", "std::vector", "String(", "readString", "dumpSize]"):
            if bad == "String(":
                # only the content-type String(F("...")) wrapper is allowed
                self.assertEqual(h.count("String("), 1)
                continue
            self.assertNotIn(bad, h)

    def test_no_raw_platform_calls_in_application_code(self):
        for f in (ROOT / "src" / "OTGW-firmware").glob("*.ino"):
            t = f.read_text(encoding="utf-8", errors="ignore")
            self.assertNotRegex(t, r"\besp_core_dump_|\besp_partition_", f.name)


@unittest.skipUnless(PLATFORM.exists(), "platform_esp32.h missing")
class CoreDumpShim(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = PLATFORM.read_text(encoding="utf-8")
        shim = cut(src, "inline size_t platformCoreDumpSize()", "// platformRestartDHCP() removed")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + shim + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.r = {l.split("|")[0]: l.split("|")[1:] for l in out.splitlines()}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_size_is_the_image_not_the_partition(self):
        self.assertEqual(self.r["size"], ["5000"])

    def test_chunked_reads_reassemble_exactly(self):
        self.assertEqual(self.r["reassembled"], ["5000", "1"])

    def test_clamps_and_ends(self):
        self.assertEqual(self.r["past_end"], ["0"])
        self.assertEqual(self.r["last_clamped"], ["10"])

    def test_image_inside_partition(self):
        self.assertEqual(self.r["offset_image"], ["64", "1"])

    def test_overrunning_image_refused(self):
        self.assertEqual(self.r["overrun"], ["0"])

    def test_no_valid_dump(self):
        self.assertEqual(self.r["none"], ["0", "0"])

    def test_app_sha_is_lowercase_hex_of_the_dump(self):
        self.assertEqual(self.r["sha_full"], ["1", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"])
        self.assertEqual(self.r["sha_9"], ["1", "a1b2c3d4e"])

    def test_app_sha_is_bounded_by_the_buffer(self):
        self.assertEqual(self.r["sha_small"], ["1", "a1b2"])

    def test_app_sha_absent_cases(self):
        for key in ("sha_empty", "sha_nonhex", "sha_nosummary", "sha_nodump"):
            self.assertEqual(self.r[key], ["0", ""], key)


if __name__ == "__main__":
    unittest.main()
