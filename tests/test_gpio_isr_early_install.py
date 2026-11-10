#!/usr/bin/env python3
"""TASK-1763 (b): the GPIO ISR service is installed early in setup(), before startWiFi().

Background (TASK-1763, coredump of .61 beta.162): Arduino installs the GPIO ISR service on the
first attachInterrupt() of a boot (esp32-hal-gpio.c:211-240, once per boot). IDF registers the
GPIO interrupt through esp_ipc_call_blocking() on the calling core, so esp_intr_alloc() runs on
the ipc1 task stack (1024 B in the Arduino libs sdkconfig). A level-1 interrupt taken there
overflowed it (SP 52 B below the stack bottom). The mitigation (b), chosen by Robert
2026-11-10 ("(c) Both"): do the one install early in setup(), before WiFi starts and before the
first attachInterrupt() (initOTDirect -> OpenTherm::begin). It lowers the chance; a larger IPC
stack (TASK-1763 (a)) removes the cause.

Checks:
  1. Source: setup() calls platformGpioIsrServiceInstallEarly() before startWiFi(), before the
     first initOTDirect() and before initS0Count(); the helper in platform_esp32.h calls
     gpio_install_isr_service((int)ARDUINO_ISR_FLAG), the same flags as Arduino's install.
  2. Behaviour, host-compiled: the REAL __attachInterruptFunctionalArg() cut from the
     Arduino core (esp32-hal-gpio.c) and the REAL helper cut from platform_esp32.h, with the IDF
     calls stubbed. With the early call first, the IDF install runs exactly once, in the early
     call, with ARDUINO_ISR_FLAG; the later attach does not install again, and still adds its
     handler. Without the early call (the old order) the install runs inside the attach.
     Skipped when the Arduino core sources or a host C++ compiler are not found
     (PLATFORMIO_CORE_DIR, default D:/DevData/platformio, or ~/.platformio).

OTGW_SETUP_INO / OTGW_PLATFORM_H point at other copies (e.g. `git show HEAD:...`) for old-vs-fix.
Not checked: the real IPC/stack behaviour on the device (bench: repeated /ReBoot, TASK-1763 AC2).
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

SETUP_INO = Path(os.environ.get("OTGW_SETUP_INO", REPO / "src" / "OTGW-firmware" / "OTGW-firmware.ino"))
PLATFORM_H = Path(os.environ.get("OTGW_PLATFORM_H", REPO / "src" / "libraries" / "Platform" / "src" / "platform_esp32.h"))


def _core_gpio_c():
    roots = [os.environ.get("PLATFORMIO_CORE_DIR"), "D:/DevData/platformio", str(Path.home() / ".platformio")]
    for r in roots:
        if not r:
            continue
        p = Path(r) / "packages" / "framework-arduinoespressif32" / "cores" / "esp32" / "esp32-hal-gpio.c"
        if p.exists():
            return p
    return None


def _setup_body(src):
    m = re.search(r"^void setup\(\)\s*\{.*?^\}", src, re.S | re.M)
    if not m:
        raise AssertionError("setup() not found in %s" % SETUP_INO)
    return m.group(0)


def _fn(src, head):
    m = re.search(r"^" + re.escape(head) + r"\s*\{.*?^\}", src, re.S | re.M)
    return m.group(0) if m else ""


class SourceOrder(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.setup = _setup_body(SETUP_INO.read_text(encoding="utf-8").replace("\r\n", "\n"))
        cls.plat = PLATFORM_H.read_text(encoding="utf-8").replace("\r\n", "\n")

    def _call(self, name):
        # The first code line that calls name (comments mention the names too, so match statements only).
        m = re.search(r"^[^/\n]*\b" + re.escape(name) + r"\(", self.setup, re.M)
        return m.start() if m else -1

    def test_early_install_is_called_in_setup(self):
        self.assertGreaterEqual(self._call("platformGpioIsrServiceInstallEarly"), 0,
                                "setup() does not install the GPIO ISR service early (TASK-1763 (b))")

    def test_early_install_comes_before_wifi_otdirect_and_s0(self):
        i = self._call("platformGpioIsrServiceInstallEarly")
        self.assertGreaterEqual(i, 0, "early install missing")
        for later in ("startWiFi", "initOTDirect", "initS0Count"):
            j = self._call(later)
            self.assertGreaterEqual(j, 0, "%s() not found in setup()" % later)
            self.assertLess(i, j, "the early GPIO ISR install must come before %s()" % later)

    def test_helper_uses_the_arduino_flags(self):
        body = _fn(self.plat, "inline esp_err_t platformGpioIsrServiceInstallEarly()")
        self.assertTrue(body, "platformGpioIsrServiceInstallEarly() not found in platform_esp32.h")
        self.assertIn("gpio_install_isr_service((int)ARDUINO_ISR_FLAG)", body)


STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NO_MEM 0x101
#define ARDUINO_ISR_FLAG 0x400            // stand-in for ESP_INTR_FLAG_IRAM
#define SOC_GPIO_PIN_COUNT 49
#define log_e(...) ((void)0)
typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void *);
typedef int gpio_num_t;
typedef int gpio_int_type_t;
typedef void (*gpio_isr_t)(void *);
typedef struct { voidFuncPtr fn; void *arg; bool functional; } InterruptHandle_t;
static InterruptHandle_t __pinInterruptHandlers[SOC_GPIO_PIN_COUNT];
static void cleanupFunctional(void *) {}
static void __onPinInterrupt(void *) {}
typedef struct { void *dev; } gpio_hal_context_t;
#define GPIO_LL_GET_HW(x) ((void *)0)
#define GPIO_PORT_0 0
static void gpio_hal_input_enable(gpio_hal_context_t *, int) {}
static int g_installs = 0, g_installFlags = -1, g_installPhase = -1, g_phase = 0, g_handlerAdds = 0;
static bool g_installed = false;
static esp_err_t gpio_install_isr_service(int flags) {
  if (g_installed) return ESP_ERR_INVALID_STATE;          // IDF gpio.c:530
  g_installed = true; g_installs++; g_installFlags = flags; g_installPhase = g_phase;
  return ESP_OK;
}
static esp_err_t gpio_set_intr_type(gpio_num_t, gpio_int_type_t) { return ESP_OK; }
static esp_err_t gpio_wakeup_enable(gpio_num_t, gpio_int_type_t) { return ESP_OK; }
static esp_err_t gpio_isr_handler_add(gpio_num_t, gpio_isr_t, void *) { g_handlerAdds++; return ESP_OK; }
static void otIsr() {}
"""

MAIN = r"""
int main(int argc, char **argv) {
  const bool early = argc > 1 && !strcmp(argv[1], "early");
  g_phase = 1;                                              // setup-early
  if (early) platformGpioIsrServiceInstallEarly();
  g_phase = 2;                                              // initOTDirect -> OpenTherm::begin -> attachInterrupt
  __attachInterruptFunctionalArg(5, (voidFuncPtrArg)otIsr, NULL, 3, false);
  __attachInterruptFunctionalArg(6, (voidFuncPtrArg)otIsr, NULL, 3, false);   // the slave pin
  printf("installs=%d flags=%d phase=%d adds=%d\n", g_installs, g_installFlags, g_installPhase, g_handlerAdds);
  return 0;
}
"""


class RealAttachWithEarlyInstall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = None
        core = _core_gpio_c()
        if core is None:
            return
        attach = _fn(core.read_text(encoding="utf-8").replace("\r\n", "\n"),
                     "extern void __attachInterruptFunctionalArg(uint8_t pin, voidFuncPtrArg userFunc, void *arg, int intr_type, bool functional)")
        if not attach:
            raise AssertionError("__attachInterruptFunctionalArg not found in %s" % core)
        attach = attach.replace("extern void __attachInterruptFunctionalArg", "static void __attachInterruptFunctionalArg")
        plat = PLATFORM_H.read_text(encoding="utf-8").replace("\r\n", "\n")
        helper = _fn(plat, "inline esp_err_t platformGpioIsrServiceInstallEarly()")
        if not helper:   # old code: no helper; keep the harness compiling so the cases fail, not the build
            helper = "inline esp_err_t platformGpioIsrServiceInstallEarly() { return ESP_OK; }"
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "g.cpp"
        cpp.write_text(STUBS + attach + "\n" + helper + "\n" + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "g.exe")

    def run_case(self, mode):
        if self.exe is None:
            self.skipTest("Arduino core sources or host C++ compiler not found")
        out = subprocess.run([str(self.exe), mode], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    def test_early_call_does_the_one_install_with_the_arduino_flags(self):
        r = self.run_case("early")
        self.assertEqual((r["installs"], r["flags"], r["phase"]), ("1", str(0x400), "1"),
                         "the IDF install must run once, in the early call, with ARDUINO_ISR_FLAG: %r" % r)
        self.assertEqual(r["adds"], "2", "both attachInterrupt() calls must still add their handler: %r" % r)

    def test_old_order_installs_inside_the_first_attach(self):
        r = self.run_case("late")
        self.assertEqual((r["installs"], r["phase"], r["adds"]), ("1", "2", "2"), r)


if __name__ == "__main__":
    unittest.main()
