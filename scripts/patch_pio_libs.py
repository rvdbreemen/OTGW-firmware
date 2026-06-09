"""
PlatformIO pre-build script: patch library compatibility issues and fix tool paths.

Patch 3 — ESP32 mklittlefs PATH (pioarduino platform):
  The pioarduino ESP32 platform installs tool-mklittlefs 3.2.0 from its own
  registry, so the package directory has a src-xxx hash suffix and is NOT
  the canonical tool-mklittlefs directory (which holds the older ESP8266
  version). PlatformIO's SCons builder calls mklittlefs as a bare command
  name, relying on PATH. If only the old ESP8266 mklittlefs is in PATH (or
  none at all), buildfs fails with "not recognized". We prepend the directory
  returned by platform.get_package_dir("tool-mklittlefs") — which for the
  pioarduino platform resolves to the 3.2.0 package — so buildfs finds the
  correct binary.

Patch 1 — NetApiHelpers 1.0.2 ArduinoWiFiServer.h:
  Checks #elif defined(ESP8266) && (ARDUINO_ESP8266_MAJOR < 3), but PlatformIO's
  ESP8266 framework package always sets ARDUINO_ESP8266_MAJOR = 3 (the PACKAGE
  version major, not the logical Arduino Core version). The actual API is
  compatible with the 2.x branch, so we broaden the guard to accept all ESP8266
  targets regardless of package major number.

Patch 2 — ESP8266 core Stream.h debug include (Windows case-insensitive FS):
  Stream.h uses #include <debug.h> (angle brackets = all include paths). On
  Windows, the case-insensitive filesystem finds this project's Debug.h (capital
  D) instead of the framework's own cores/esp8266/debug.h. This causes a
  circular include chain (Stream.h → our Debug.h → platform_esp8266.h →
  ESP8266WiFi.h → Client.h: public Stream) that breaks compilation because the
  Stream class hasn't been fully defined yet when Client.h tries to derive from
  it. Patching to "debug.h" (quotes) makes the compiler search the local
  directory first — where the real debug.h lives.
"""

Import("env")  # noqa: F821  (SCons variable injected by PlatformIO)
import os

# --- Patch 3: ESP32 mklittlefs PATH fix (pioarduino platform) -----------------
# The pioarduino platform installs tool-mklittlefs 3.2.0 with a src-xxx hash
# suffix in the directory name.  PlatformIO's SCons builder calls mklittlefs
# as a bare name so it relies on PATH, but the versioned package dir is never
# added.  We prepend it here so `pio run -t buildfs` can find mklittlefs.exe.
# There can be multiple tool-mklittlefs packages; scan all of them (including
# versioned src-xxx dirs) for the one that actually contains mklittlefs.exe.
# Applies to every ESP32-S3 env on the pioarduino platform: esp32 AND the
# esp32-combo env (ADR-125), which extends esp32 and needs the same mklittlefs
# on PATH for `-t buildfs`. startswith("esp32") matches both but not esp8266
# (which uses its own, already-on-PATH ESP8266 mklittlefs).
if env.get("PIOENV", "").startswith("esp32"):
    packages_dir = env["PROJECT_PACKAGES_DIR"]
    mklittlefs_found = False
    for entry in sorted(os.listdir(packages_dir)):
        if entry.startswith("tool-mklittlefs"):
            candidate = os.path.join(packages_dir, entry)
            exe = os.path.join(candidate, "mklittlefs.exe")
            if os.path.isfile(exe):
                env.PrependENVPath("PATH", candidate)
                print(f"patch_pio_libs: ESP32 mklittlefs PATH: added {candidate}")
                mklittlefs_found = True
                break
    if not mklittlefs_found:
        print("patch_pio_libs: ESP32 mklittlefs PATH: mklittlefs.exe not found in any tool-mklittlefs* package")

def patch_file(path, old, new, description):
    if not os.path.exists(path):
        print(f"patch_pio_libs: {description}: file not found, skipping ({path})")
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    if old not in content:
        if new in content:
            print(f"patch_pio_libs: {description}: already patched")
        else:
            print(f"patch_pio_libs: {description}: pattern not found — check library version")
        return
    patched = content.replace(old, new)
    with open(path, "w", encoding="utf-8") as f:
        f.write(patched)
    print(f"patch_pio_libs: {description}: patched OK")

# --- Patch 1: NetApiHelpers ArduinoWiFiServer.h ----------------------------
# PlatformIO ESP8266 framework sets ARDUINO_ESP8266_MAJOR=3 (package version),
# but the API is the same as arduino-cli Core 2.x. Widen the guard to cover it.
arduino_wifi_server = os.path.join(
    env["PROJECT_LIBDEPS_DIR"],
    env["PIOENV"],
    "NetApiHelpers",
    "src",
    "ArduinoWiFiServer.h",
)
patch_file(
    arduino_wifi_server,
    "#elif defined(ESP8266) && (ARDUINO_ESP8266_MAJOR < 3)",
    "#elif defined(ESP8266)  /* PlatformIO pkg MAJOR is always 3; API matches 2.x */",
    "NetApiHelpers ArduinoWiFiServer.h ESP8266 major guard",
)
# Sub-patch 1b (repair): restore SERVER_DONT_INHERIT_FROM_PRINT if a previous
# incorrect build script removed it. On Core 2.7.4 (espressif8266 package 3.x)
# WiFiServer still inherits from Print. ServerTemplate<WiFiServer,WiFiClient>
# must NOT also inherit Print (diamond → ambiguity warning at ServerTemplate.h:37).
# The define instructs ServerTemplate to skip its own Print base.
# Note: a fresh library download already has the define; the patch_file helper
# will skip silently ("pattern not found") in that case.
patch_file(
    arduino_wifi_server,
    "/* PlatformIO pkg MAJOR is always 3; Core 3.x WiFiServer no longer inherits Print */\n#define SERVER_CTOR_WITH_IP",
    "/* PlatformIO pkg MAJOR is always 3; WiFiServer (Core 2.7.4) inherits Print, guard ServerTemplate */\n#define SERVER_DONT_INHERIT_FROM_PRINT\n#define SERVER_CTOR_WITH_IP",
    "NetApiHelpers ArduinoWiFiServer.h restore SERVER_DONT_INHERIT_FROM_PRINT for Core 2.7.4",
)

# --- Patch 2: ESP8266 core Stream.h debug include (Windows case-insensitive FS) ---
# Stream.h uses #include <debug.h> (angle-brackets = searches all include paths).
# On Windows, the case-insensitive FS finds this project's Debug.h (capital D)
# instead of the framework's own cores/esp8266/debug.h, triggering a circular
# include chain that breaks Stream class compilation.
# Changing to "debug.h" (quotes) makes the compiler look in the SAME directory
# as Stream.h first — the cores/esp8266/ dir — where the real debug.h lives.
import os as _os
_framework_root = _os.path.join(
    env["PROJECT_PACKAGES_DIR"],
    "framework-arduinoespressif8266",
    "cores",
    "esp8266",
)
_debug_include_old = "#include <debug.h>"
_debug_include_new = '#include "debug.h"  /* patched: quotes → searches local dir first, avoids Windows CI-FS picking up project Debug.h */'

stream_h = _os.path.join(_framework_root, "Stream.h")
patch_file(stream_h, _debug_include_old, _debug_include_new,
           "ESP8266 core Stream.h debug include (Windows CI-FS guard)")

# LittleFS.h also uses #include <debug.h>; same fix needed
littlefs_h = _os.path.join(
    env["PROJECT_PACKAGES_DIR"],
    "framework-arduinoespressif8266",
    "libraries", "LittleFS", "src", "LittleFS.h",
)
patch_file(littlefs_h, _debug_include_old, _debug_include_new,
           "ESP8266 LittleFS.h debug include (Windows CI-FS guard)")
