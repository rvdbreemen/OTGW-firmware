#!/usr/bin/env python3
"""TASK-1870: host gate for ADR-225, our copy of IDF esp_ipc.c with a 1280 B IPC task stack.

src/OTGW-firmware/esp_ipc_stack.c replaces libesp_system.a(esp_ipc.c.obj) from the prebuilt
Arduino libs, so the copy must stay identical to the IDF source those libs were built from,
apart from the marked patch block. This test checks:

  1. The file exists (red on the code from before ADR-225).
  2. With the OTGW-PATCH block put back to the upstream lines, the file hashes to IDF
     components/esp_system/esp_ipc.c at commit 9bb7aa84fe (release/v5.5), line endings ignored.
  3. The patch block sets a stack of at least 1280 B and keeps the #error guards on the IDF
     version (5.5.1) and on the prebuilt CONFIG_ESP_IPC_TASK_STACK_SIZE (1024).
  4. platformio.ini still pins pioarduino 55.03.35, the platform that ships those libs.
  5. When the PlatformIO packages are on this machine: the libs' versions.txt names IDF
     9bb7aa84fe, and the local framework-espidf esp_ipc.c (when present) hashes the same.
     Without the packages these cases are skipped, they do not pass silently.

The version number alone cannot tell two release/v5.5 snapshots apart (the libs package.json says
5.5.0+sha.9bb7aa84fe while the headers say 5.5.1), so the commit and the hash carry the check.
MutantGate builds mutants at test time and asserts the same checks go red on them.

The ELF side (the linker really took our object) is scripts/check_esp_ipc_override.py.
Run: python tests/test_esp_ipc_override_gate.py
"""
import hashlib
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = Path(os.environ.get("OTGW_ESP_IPC_C", REPO / "src" / "OTGW-firmware" / "esp_ipc_stack.c"))
PIO_INI = REPO / "platformio.ini"

UPSTREAM_SHA256 = "1429b6843566356758c9e4c80ecfcdc4c01c1bf3ad20dc4bfe534d41ece7414f"
IDF_COMMIT = "9bb7aa84fe"
PLATFORM_PIN = "pioarduino/platform-espressif32/releases/download/55.03.35/"
UPSTREAM_BLOCK = (
    "#if CONFIG_COMPILER_OPTIMIZATION_NONE\n"
    "#define IPC_STACK_SIZE (CONFIG_ESP_IPC_TASK_STACK_SIZE + 0x100)\n"
    "#else\n"
    "#define IPC_STACK_SIZE (CONFIG_ESP_IPC_TASK_STACK_SIZE)\n"
    "#endif //CONFIG_COMPILER_OPTIMIZATION_NONE\n"
)
PATCH_RE = re.compile(r"// OTGW-PATCH-BEGIN[^\n]*\n.*?// OTGW-PATCH-END\n", re.S)


def norm(text):
    return text.replace("\r\n", "\n")


def check_source(text):
    """Return a list of problems with the override source; empty when it passes."""
    text = norm(text)
    blocks = PATCH_RE.findall(text)
    if len(blocks) != 1:
        return [f"expected one OTGW-PATCH block, found {len(blocks)}"]
    probs = []
    restored = PATCH_RE.sub(lambda _m: UPSTREAM_BLOCK, text)
    got = hashlib.sha256(restored.encode("utf-8")).hexdigest()
    if got != UPSTREAM_SHA256:
        probs.append(f"outside the patch block the file differs from IDF {IDF_COMMIT} esp_ipc.c ({got[:12]})")
    blk = blocks[0]
    m = re.search(r"#define\s+OTGW_IPC_TASK_STACK_SIZE\s+(\d+)", blk)
    if not m or int(m.group(1)) < 1280:
        probs.append("OTGW_IPC_TASK_STACK_SIZE missing or below 1280")
    if not re.search(r"#define\s+IPC_STACK_SIZE\s+\(OTGW_IPC_TASK_STACK_SIZE\)", blk):
        probs.append("IPC_STACK_SIZE does not use OTGW_IPC_TASK_STACK_SIZE")
    if "CONFIG_ESP_IPC_TASK_STACK_SIZE)" in blk.replace("CONFIG_ESP_IPC_TASK_STACK_SIZE != 1024", ""):
        probs.append("patch block still sizes the stack from CONFIG_ESP_IPC_TASK_STACK_SIZE")
    if not re.search(r"#if\s+ESP_IDF_VERSION\s*!=\s*ESP_IDF_VERSION_VAL\(5,\s*5,\s*1\)\s*\n#error", blk):
        probs.append("no #error guard on IDF 5.5.1")
    if not re.search(r"#if\s+CONFIG_ESP_IPC_TASK_STACK_SIZE\s*!=\s*1024\s*\n#error", blk):
        probs.append("no #error guard on the prebuilt CONFIG_ESP_IPC_TASK_STACK_SIZE 1024")
    return probs


def pio_packages():
    roots = []
    if os.environ.get("PLATFORMIO_CORE_DIR"):
        roots.append(Path(os.environ["PLATFORMIO_CORE_DIR"]))
    roots += [Path("D:/DevData/platformio"), Path.home() / ".platformio"]
    for r in roots:
        p = r / "packages"
        if (p / "framework-arduinoespressif32-libs").is_dir():
            return p
    return None


class SourceGate(unittest.TestCase):
    def test_override_file_exists(self):
        self.assertTrue(SRC.is_file(), f"{SRC} missing: the IPC stack stays at the prebuilt 1024 B (ADR-225)")

    def test_source_matches_upstream_and_patch(self):
        self.assertTrue(SRC.is_file(), f"{SRC} missing")
        self.assertEqual(check_source(SRC.read_text(encoding="utf-8")), [])

    def test_platform_pin(self):
        ini = PIO_INI.read_text(encoding="utf-8")
        self.assertIn(PLATFORM_PIN, ini,
                      "the pioarduino platform moved: re-check esp_ipc_stack.c against the new libs' IDF (ADR-225)")


class InstalledPackages(unittest.TestCase):
    def setUp(self):
        self.pkgs = pio_packages()
        if self.pkgs is None:
            self.skipTest("PlatformIO packages not on this machine")

    def test_libs_idf_commit(self):
        txt = (self.pkgs / "framework-arduinoespressif32-libs" / "versions.txt").read_text(encoding="utf-8")
        m = re.search(r"^esp-idf:\s*\S+\s+([0-9a-f]+)", txt, re.M)
        self.assertIsNotNone(m, "no esp-idf line in versions.txt")
        self.assertEqual(m.group(1), IDF_COMMIT,
                         "the prebuilt libs come from another IDF commit: copy esp_ipc.c again (ADR-225)")

    def test_local_espidf_copy_matches(self):
        p = self.pkgs / "framework-espidf" / "components" / "esp_system" / "esp_ipc.c"
        if not p.is_file():
            self.skipTest("framework-espidf not installed")
        got = hashlib.sha256(norm(p.read_text(encoding="utf-8")).encode("utf-8")).hexdigest()
        self.assertEqual(got, UPSTREAM_SHA256, "local framework-espidf esp_ipc.c differs from the pinned upstream")


class MutantGate(unittest.TestCase):
    def setUp(self):
        if not SRC.is_file():
            self.skipTest("override missing (SourceGate reports it)")
        self.text = norm(SRC.read_text(encoding="utf-8"))

    def test_lower_stack_goes_red(self):
        m = self.text.replace("#define OTGW_IPC_TASK_STACK_SIZE 1280", "#define OTGW_IPC_TASK_STACK_SIZE 1024")
        self.assertNotEqual(m, self.text)
        self.assertTrue(check_source(m))

    def test_upstream_edit_outside_block_goes_red(self):
        m = self.text.replace("#define IPC_MAX_PRIORITY (configMAX_PRIORITIES - 1)",
                              "#define IPC_MAX_PRIORITY (configMAX_PRIORITIES - 2)")
        self.assertNotEqual(m, self.text)
        self.assertTrue(any("differs from IDF" in p for p in check_source(m)))

    def test_missing_version_guard_goes_red(self):
        m = re.sub(r"#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL\(5, 5, 1\)\n#error[^\n]*\n#endif\n", "", self.text)
        self.assertNotEqual(m, self.text)
        self.assertTrue(any("5.5.1" in p for p in check_source(m)))

    def test_prebuilt_config_stack_goes_red(self):
        m = self.text.replace("#define IPC_STACK_SIZE (OTGW_IPC_TASK_STACK_SIZE)\n",
                              "#define IPC_STACK_SIZE (CONFIG_ESP_IPC_TASK_STACK_SIZE)\n")
        self.assertNotEqual(m, self.text)
        self.assertTrue(check_source(m))

    def test_crlf_checkout_still_passes(self):
        self.assertEqual(check_source(self.text.replace("\n", "\r\n")), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
