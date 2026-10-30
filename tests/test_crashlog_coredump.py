#!/usr/bin/env python3
"""TASK-1479: an ESP32 panic or task-watchdog reset shows up in the crash log.

Before the fix platformResetRegisterDump() was a no-op on ESP32, so updateRebootLog() never
wrote a detail line, and readLatestCrashLog() only accepted the ESP8266 detail prefixes
("ESP register contents:", "External Reason:"). /api/v2/device/crashlog therefore stayed
available:false after every ESP32 crash, also after a Task-watchdog reset.

The code under test is NOT re-implemented. The test cuts out of the real sources:
- platformResetRegisterDump() from src/libraries/Platform/src/platform_esp32.h, and
- isRebootLogSummaryLine() .. readLatestCrashLog() from src/OTGW-firmware/helperStuff.ino,
and compiles them on the host. The ESP-IDF core-dump calls, esp_reset_reason(), Preferences
(NVS) and LittleFS are stubs whose values each scenario sets.

OTGW_PLATFORM_H and OTGW_HELPER_INO point at other copies (e.g. `git show <rev>:<path>`) for
old-vs-fix runs.

Run: python tests/test_crashlog_coredump.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
PLATFORM_H = Path(os.environ.get("OTGW_PLATFORM_H", REPO / "src" / "libraries" / "Platform" / "src" / "platform_esp32.h"))
HELPER_INO = Path(os.environ.get("OTGW_HELPER_INO", REPO / "src" / "OTGW-firmware" / "helperStuff.ino"))

STUBS = r"""
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <map>
#define PSTR(s) (s)
#define strncmp_P strncmp
#define snprintf_P snprintf
static size_t strlcpy(char *d, const char *s, size_t n) {
  size_t l = strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; }
  return l;
}

// ---- ESP-IDF stubs ----
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
typedef enum { ESP_RST_UNKNOWN = 0, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC,
               ESP_RST_INT_WDT, ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_DEEPSLEEP,
               ESP_RST_BROWNOUT, ESP_RST_SDIO } esp_reset_reason_t;
typedef struct { uint32_t bt[16]; uint32_t depth; bool corrupted; } esp_core_dump_bt_info_t;
typedef struct { uint32_t exc_tcb; char exc_task[16]; uint32_t exc_pc; esp_core_dump_bt_info_t exc_bt_info;
                 uint32_t core_dump_version; uint8_t app_elf_sha256[10]; } esp_core_dump_summary_t;
static bool g_haveDump = false;
static esp_core_dump_summary_t g_sum;
static size_t g_dumpSize = 0;
static esp_reset_reason_t g_reason = ESP_RST_POWERON;
esp_reset_reason_t esp_reset_reason() { return g_reason; }
esp_err_t esp_core_dump_image_check() { return g_haveDump ? ESP_OK : ESP_FAIL; }
esp_err_t esp_core_dump_image_get(size_t *a, size_t *s) { if (!g_haveDump) return ESP_FAIL; *a = 0x3F0000; *s = g_dumpSize; return ESP_OK; }
esp_err_t esp_core_dump_get_summary(esp_core_dump_summary_t *s) { if (!g_haveDump) return ESP_FAIL; *s = g_sum; return ESP_OK; }

// ---- Preferences (NVS) stub ----
static std::map<std::string, uint32_t> g_nvs;
struct Preferences {
  std::string ns;
  bool begin(const char *n, bool) { ns = n; return true; }
  uint32_t getUInt(const char *k, uint32_t d) { auto it = g_nvs.find(ns + "/" + k); return it == g_nvs.end() ? d : it->second; }
  size_t putUInt(const char *k, uint32_t v) { g_nvs[ns + "/" + k] = v; return 4; }
  void end() {}
};

// ---- LittleFS stub ----
bool LittleFSmounted = true;
static std::string g_file;
#define F(s) (s)
struct File {
  std::string data; size_t pos = 0; bool ok = false; std::string out;
  explicit operator bool() const { return ok; }
  bool available() { return pos < data.size(); }
  int read() { return pos < data.size() ? (unsigned char)data[pos++] : -1; }
  int peek() { return pos < data.size() ? (unsigned char)data[pos] : -1; }
  void print(const char *s) { out += s; }
  size_t readBytesUntil(char t, char *buf, size_t len) {
    size_t n = 0;
    while (pos < data.size() && n < len) { char c = data[pos++]; if (c == t) break; buf[n++] = c; }
    return n;
  }
  void close() {}
};
struct { File open(const char *, const char *) { File f; f.ok = true; f.data = g_file; return f; } } LittleFS;
"""

MAIN = r"""
static void dumpLine(const char *tag) {
  char buf[120];   // updateRebootLog(): char log_line_regs[120]
  memset(buf, 'X', sizeof(buf));
  platformResetRegisterDump(buf, sizeof(buf));
  std::string s(buf);
  std::string esc;
  for (char c : s) { if (c == '\r') esc += "\\r"; else if (c == '\n') esc += "\\n"; else esc += c; }
  printf("%s|%zu|%s\n", tag, strlen(buf), esc.c_str());
}

static void fillDump(uint32_t pc, uint32_t depth, bool corrupted) {
  memset(&g_sum, 0, sizeof(g_sum));
  strcpy(g_sum.exc_task, "loopTask");
  g_sum.exc_pc = pc;
  for (uint32_t i = 0; i < 16; i++) g_sum.exc_bt_info.bt[i] = 0x42000000u + i * 0x111u;
  g_sum.exc_bt_info.depth = depth;
  g_sum.exc_bt_info.corrupted = corrupted;
  memcpy(g_sum.app_elf_sha256, "abcdef012", 10);   // CONFIG_APP_RETRIEVE_LEN_ELF_SHA = 9 hex chars + NUL
  g_dumpSize = 0x6000;
  g_haveDump = true;
}

int main() {
  // A task-watchdog crash left a dump; first boot after it reports it, the next boot does not.
  g_reason = ESP_RST_TASK_WDT;
  fillDump(0x42012345u, 16, false);
  dumpLine("first");
  dumpLine("again");
  // An OTA after the crash: software reset, but a new (other) dump is still unreported.
  g_reason = ESP_RST_SW;
  fillDump(0x42054321u, 3, true);
  dumpLine("ota");
  // Watchdog reset with no readable dump.
  g_haveDump = false;
  g_reason = ESP_RST_TASK_WDT;
  dumpLine("nodump");
  // Normal software reset, no dump: no detail line.
  g_reason = ESP_RST_SW;
  dumpLine("clean");

  // The crash-log reader on a reboot_log.txt whose newest entry has a core-dump detail line.
  char summary[160], details[160];
  g_file = "2026-10-29 12:56:52 - reboot cause: Task watchdog (6) \r\n"
           "Core dump: loopTask pc=0x42012345 bt=0x42000000 0x42000111\r\n"
           "2026-10-29 08:03:22 - reboot cause: Software reset (3) \r\n";
  bool ok = readLatestCrashLog(summary, sizeof(summary), details, sizeof(details));
  printf("parse|%d|%s|%s\n", ok ? 1 : 0, summary, details);

#ifndef NO_COPY_FN
  // The copy of older lines in updateRebootLog(): a file from older firmware has its older
  // entries joined by '\r' only (seen on the bench unit). 25 entries, keep at most 19.
  File in, out;
  in.ok = out.ok = true;
  in.data = "2026-10-29 12:00:00 - reboot cause: Power on (1) \r\n";
  for (int k = 0; k < 24; k++) {
    char e[80];
    snprintf(e, sizeof(e), "2026-10-%02d 01:00:00 - reboot cause: Software reset (3) \r", k + 1);
    in.data += e;
  }
  copyOlderRebootLogLines(in, out, 19);
  int crlf = 0, lone = 0;
  for (size_t k = 0; k < out.out.size(); k++) {
    if (out.out[k] == '\r') { if (k + 1 < out.out.size() && out.out[k + 1] == '\n') crlf++; else lone++; }
  }
  printf("copy|%d|%d|%zu\n", crlf, lone, out.out.size());
#endif
  return 0;
}
"""


def _slice(text, start, end, name):
    a = text.find(start)
    b = text.find(end, a + 1)
    if a < 0 or b < 0:
        raise AssertionError(f"anchor not found in {name}: {start!r} .. {end!r}")
    return text[a:b]


class CrashlogCoreDump(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plat = PLATFORM_H.read_text(encoding="utf-8")
        helper = HELPER_INO.read_text(encoding="utf-8")
        shim = _slice(plat, "inline void platformResetRegisterDump", "// Exception cause", PLATFORM_H)
        parser = _slice(helper, "static bool isRebootLogSummaryLine", "bool updateRebootLog(", HELPER_INO)
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cls.has_copy_fn = "copyOlderRebootLogLines" in parser
        guard = "" if cls.has_copy_fn else "#define NO_COPY_FN\n"
        cpp.write_text(guard + STUBS + shim + "\n" + parser + "\n" + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.rows = {}
        for line in out.splitlines():
            parts = line.split("|", 3)
            cls.rows[parts[0]] = parts[1:]
        print(out)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_dump_is_reported_with_task_and_backtrace(self):
        n, text = self.rows["first"]
        self.assertTrue(text.startswith("Core dump: loopTask pc=0x42012345 elf=abcdef01 bt=0x42000000 0x42000111"), text)
        self.assertTrue(text.endswith("\\r\\n"), text)
        self.assertLess(int(n), 120)

    def test_same_dump_is_reported_once(self):
        self.assertEqual(self.rows["again"], ["0", ""])

    def test_new_dump_reported_after_software_reset(self):
        n, text = self.rows["ota"]
        self.assertTrue(text.startswith("Core dump: loopTask pc=0x42054321 elf=abcdef01 bt="), text)
        self.assertTrue(text.endswith("!\\r\\n"), text)   # corrupted backtrace marker

    def test_watchdog_reset_without_dump(self):
        self.assertEqual(self.rows["nodump"][1], "Core dump: none (reset 6)\\r\\n")

    def test_clean_reset_writes_nothing(self):
        self.assertEqual(self.rows["clean"], ["0", ""])

    def test_older_lines_get_their_line_breaks_back(self):
        if not self.has_copy_fn:
            self.skipTest("copyOlderRebootLogLines() not in this source (old code: the copy loop is inline "
                          "in updateRebootLog(); the old behaviour is evidenced by a bench reboot_log.txt)")
        crlf, lone, _ = self.rows["copy"]
        self.assertEqual(crlf, "19")   # 19 lines kept, each ends in \r\n
        self.assertEqual(lone, "0")    # no line joined by a bare \r any more

    def test_crashlog_reader_accepts_core_dump_line(self):
        ok, summary, details = self.rows["parse"]
        self.assertEqual(ok, "1")
        self.assertIn("reboot cause: Task watchdog (6)", summary)
        self.assertTrue(details.startswith("Core dump: loopTask"), details)


if __name__ == "__main__":
    unittest.main(verbosity=2)
