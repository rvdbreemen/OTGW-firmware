"""TASK-1819: device/info "firmware_ota" names how a firmware image reaches the unit.

Compiles the real hasSpareAppOtaSlot(), safebootPartition(), hasSafeboot(),
firmwareOtaPathFor() and firmwareOtaPath() from OTGW-ModUpdateServer-esp32.h
against a stub partition table. The stub's esp_ota_get_next_update_partition()
follows IDF's algorithm (components/app_update/esp_ota_ops.c,
esp_ota_get_next_update_partition: the first OTA slot is the default, the slot
after the running one wins), because that is what makes the one-slot case
return the running partition. The IDF function itself is not compiled here.

OTGW_UPDSRV_H points at another copy of the header to see the old code red.

Run: python -m pytest -q tests/test_firmware_ota_path.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = HERE.parent / "src" / "OTGW-firmware"
HDR = Path(os.environ.get("OTGW_UPDSRV_H", SRC / "OTGW-ModUpdateServer-esp32.h"))
REST = SRC / "restAPI.ino"

FUNCS = ("hasSpareAppOtaSlot", "safebootPartition", "hasSafeboot", "firmwareOtaPathFor", "firmwareOtaPath")


def extract(text: str, name: str) -> str:
    m = re.search(r"^inline [^\n(]*\b" + name + r"\(", text, re.M)
    if not m:
        return ""
    if text[m.start():].split("\n", 1)[0].rstrip().endswith("}"):
        return text[m.start():].split("\n", 1)[0] + "\n"
    depth, i = 0, text.index("{", m.start())
    for j in range(i, len(text)):
        depth += {"{": 1, "}": -1}.get(text[j], 0)
        if depth == 0:
            return text[m.start():j + 1] + "\n"
    return ""


PRELUDE = r"""
#include <cstdio>
#include <cstring>
#include <vector>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
enum { ESP_PARTITION_TYPE_APP = 0 };
enum { ESP_PARTITION_SUBTYPE_APP_FACTORY = 0x00, ESP_PARTITION_SUBTYPE_APP_OTA_0 = 0x10,
       ESP_PARTITION_SUBTYPE_APP_OTA_MAX = 0x20 };
struct esp_partition_t { int type; int subtype; const char *label; bool hasApp; };
struct esp_app_desc_t { int magic; };
static std::vector<esp_partition_t> g_table; static int g_running = 0;
static const esp_partition_t *esp_ota_get_running_partition() { return &g_table[g_running]; }
static const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *) {
  for (auto &p : g_table) if (p.type == type && p.subtype == subtype) return &p;
  return nullptr;
}
// IDF esp_ota_ops.c esp_ota_get_next_update_partition(), same steps.
static const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *start_from) {
  const esp_partition_t *default_ota = nullptr; bool next_is_result = false;
  if (start_from == nullptr) start_from = esp_ota_get_running_partition();
  for (int t = ESP_PARTITION_SUBTYPE_APP_OTA_0; t != ESP_PARTITION_SUBTYPE_APP_OTA_MAX; t++) {
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, t, nullptr);
    if (p == nullptr) continue;
    if (default_ota == nullptr) default_ota = p;
    if (p == start_from) next_is_result = true;
    else if (next_is_result) return p;
  }
  return default_ota;
}
static esp_err_t esp_ota_get_partition_description(const esp_partition_t *p, esp_app_desc_t *d) {
  if (!p->hasApp) return ESP_FAIL; d->magic = 1; return ESP_OK;
}
"""

MAIN = r"""
static void run(const char *name, std::vector<esp_partition_t> t, int running) {
  g_table = t; g_running = running;
  printf("%s app=%d sb=%d path=%s\n", name, (int)hasSpareAppOtaSlot(), (int)hasSafeboot(), firmwareOtaPath());
}
int main() {
  const esp_partition_t SB  = {0, ESP_PARTITION_SUBTYPE_APP_FACTORY, "safeboot", true};
  const esp_partition_t SBE = {0, ESP_PARTITION_SUBTYPE_APP_FACTORY, "safeboot", false};
  const esp_partition_t A0  = {0, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app0", true};
  const esp_partition_t A1  = {0, ESP_PARTITION_SUBTYPE_APP_OTA_0 + 1, "app1", true};
  run("safeboot_layout", {SB, A0}, 1);         // ADR-191 table, main firmware in app0
  run("old_layout", {A0}, 0);                  // pre-alpha.425 table: app0 only
  run("empty_factory", {SBE, A0}, 1);          // factory partition without an app image
  run("dual_slot", {A0, A1}, 0);               // a table with a spare OTA slot
  return 0;
}
"""


class FirmwareOtaPath(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        text = HDR.read_text(encoding="utf-8")
        bodies = {n: extract(text, n) for n in FUNCS}
        cls.missing = [n for n, b in bodies.items() if not b]
        cls.rows = {}
        if cls.missing:
            return
        with tempfile.TemporaryDirectory() as d:
            cpp = Path(d) / "t.cpp"
            cpp.write_text(PRELUDE + "".join(bodies[n] for n in FUNCS) + MAIN, encoding="utf-8")
            exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
            if exe is None:
                raise unittest.SkipTest("no C++ compiler")
            out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        for line in out.splitlines():
            name, *kv = line.split()
            cls.rows[name] = dict(x.split("=") for x in kv)

    def test_helpers_exist(self):
        self.assertEqual(self.missing, [], "firmware OTA path helper missing in the header")

    def test_safeboot_layout(self):
        self.assertEqual(self.rows.get("safeboot_layout"), {"app": "0", "sb": "1", "path": "safeboot"})

    def test_old_layout(self):
        self.assertEqual(self.rows.get("old_layout"), {"app": "0", "sb": "0", "path": "usb"})

    def test_empty_factory_is_not_safeboot(self):
        self.assertEqual(self.rows.get("empty_factory"), {"app": "0", "sb": "0", "path": "usb"})

    def test_dual_slot(self):
        self.assertEqual(self.rows.get("dual_slot"), {"app": "1", "sb": "0", "path": "direct"})

    def test_device_info_emits_the_path(self):
        rest = REST.read_text(encoding="utf-8")
        self.assertRegex(rest, r'je\.field\(F\("firmware_ota"\),\s*firmwareOtaPath\(\)\)')

    def test_guard_still_refuses_before_any_write(self):
        text = HDR.read_text(encoding="utf-8")
        i = text.find("if (!hasSpareAppOtaSlot()) {")
        self.assertGreater(i, 0, "the firmware-image guard is gone")
        self.assertLess(i, text.find("Update.begin(uploadTotal", i), "the guard must run before Update.begin()")
        block = text[i:text.find("Update.begin(uploadTotal", i)]
        self.assertRegex(block, r"\n\s*return;\s*\n\s*\}", "the guard no longer returns before the write")


if __name__ == "__main__":
    unittest.main()
