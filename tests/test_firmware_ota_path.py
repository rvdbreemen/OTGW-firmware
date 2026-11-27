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

FUNCS = ("hasSpareAppOtaSlot", "safebootPartition", "hasSafeboot", "firmwareOtaPathFor", "firmwareOtaPath",
         "partitionLayoutFor", "partitionLayout", "partitionLayoutExpected")


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
#ifdef _MSC_VER
#define __attribute__(x)
#endif
#include <cstdio>
#include <cstring>
#include <vector>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
enum { ESP_PARTITION_TYPE_APP = 0 };
enum { ESP_PARTITION_SUBTYPE_APP_FACTORY = 0x00, ESP_PARTITION_SUBTYPE_APP_OTA_0 = 0x10,
       ESP_PARTITION_SUBTYPE_APP_OTA_MAX = 0x20 };
struct esp_partition_t { int type; int subtype; const char *label; bool hasApp; unsigned size = 0; };
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
static void runL(const char *name, std::vector<esp_partition_t> t, int running) {
  g_table = t; g_running = running;
  printf("%s layout=%s expected=%s\n", name, partitionLayout(), partitionLayoutExpected());
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
  // ADR-298: the table is told by the size of app0, not by the partition names
  const esp_partition_t A0old = {0, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app0", true, 0x230000};
  const esp_partition_t A0new = {0, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app0", true, 0x240000};
  const esp_partition_t A0odd = {0, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app0", true, 0x250000};
  runL("layout_old_safeboot", {SB, A0old}, 1);
  runL("layout_new_safeboot", {SB, A0new}, 1);
  runL("layout_odd", {SB, A0odd}, 1);
  runL("layout_legacy", {A0old}, 0);
  runL("layout_empty_factory", {SBE, A0new}, 1);
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
            # the macro and the marker array partitionLayoutExpected() reads, taken from the header
            decl = "\n".join(re.findall(r"^(?:#define OTGW_PARTITION_LAYOUT .*|static const char kPartitionLayoutMarker.*)$",
                                         text, re.M)) + "\n"
            cpp.write_text(PRELUDE + decl + "".join(bodies[n] for n in FUNCS) + MAIN, encoding="utf-8")
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

    def test_the_layout_is_told_by_the_app0_size_not_by_the_names(self):
        self.assertEqual(self.rows.get("layout_old_safeboot", {}).get("layout"), "safeboot")
        self.assertEqual(self.rows.get("layout_new_safeboot", {}).get("layout"), "safeboot2")
        self.assertEqual(self.rows.get("layout_odd", {}).get("layout"), "unknown")
        self.assertEqual(self.rows.get("layout_legacy", {}).get("layout"), "legacy")
        self.assertEqual(self.rows.get("layout_empty_factory", {}).get("layout"), "legacy")

    def test_device_info_emits_the_layout_and_the_expected_one(self):
        rest = REST.read_text(encoding="utf-8")
        self.assertRegex(rest, r'je\.field\(F\("partition_layout"\),\s*partitionLayout\(\)\)')
        self.assertRegex(rest, r'je\.field\(F\("partition_layout_expected"\),\s*partitionLayoutExpected\(\)\)')

    def test_the_expected_layout_matches_the_partition_csv(self):
        csv = (HERE.parent / "partitions_otgw_esp32.csv").read_text(encoding="utf-8")
        row = [l for l in csv.splitlines() if l.startswith("app0")][0]
        size = int(row.split(",")[4].strip(), 16)
        name = {0x230000: "safeboot", 0x240000: "safeboot2"}[size]
        self.assertEqual(self.rows["layout_old_safeboot"]["expected"], name,
                         "partitionLayoutExpected() is not the table partitions_otgw_esp32.csv describes")

    def test_a_filesystem_image_smaller_than_the_partition_is_refused_before_any_write(self):
        text = HDR.read_text(encoding="utf-8")
        m = re.search(r"\} else if \(uploadTotal < fsSize\) \{(.*?)\} else if \(!Update\.begin\(fsSize", text, re.S)
        self.assertIsNotNone(m, "no refusal of an image that is smaller than the partition (another table's image)")
        self.assertIn("_dropKeptFiles();", m.group(1))
        self.assertIn("fsOtaRtcWipe();", m.group(1))
        self.assertIn("_updaterError", m.group(1))

    def test_a_filesystem_upload_without_size_is_refused_before_any_write(self):
        text = HDR.read_text(encoding="utf-8")
        m = re.search(r"if \(uploadTotal == 0\) \{(.*?)\} else if \(uploadTotal > fsSize\)", text, re.S)
        self.assertIsNotNone(m, "an upload without size= can be written unchecked (ADR-298 review F2)")
        self.assertIn("size=", m.group(1))
        self.assertIn("_dropKeptFiles();", m.group(1))
        self.assertIn("fsOtaRtcWipe();", m.group(1))

    def test_the_app_image_carries_the_layout_it_is_built_for(self):
        text = HDR.read_text(encoding="utf-8")
        self.assertRegex(text, r'static const char kPartitionLayoutMarker\[\] __attribute__\(\(used\)\) = "otgw-layout=" OTGW_PARTITION_LAYOUT;')
        # the page and ota_flash.py read it from the image; the expected layout is derived from the marker so it stays linked
        self.assertIn("kPartitionLayoutMarker + (sizeof(\"otgw-layout=\") - 1)", text)

    def test_the_first_boot_on_another_table_says_so(self):
        ino = (SRC / "OTGW-firmware.ino").read_text(encoding="utf-8")
        self.assertRegex(ino, r"strcmp\(partitionLayout\(\), partitionLayoutExpected\(\)\) != 0\)\s*SetupDebugf")

    def test_the_update_page_blocks_the_one_action_update_on_a_layout_mismatch(self):
        page = (SRC / "webui" / "update.html").read_text(encoding="utf-8")
        self.assertIn("d.partition_layout_expected", page)
        self.assertRegex(page, r"s\.layout !== s\.layoutWant\) \{\s*blockOne\(")

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
