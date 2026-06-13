"""
PlatformIO pre-build script: fix tool paths for the ESP32-S3 build.

Patch 3 — ESP32 mklittlefs PATH (pioarduino platform):
  The pioarduino ESP32 platform installs tool-mklittlefs 3.2.0 from its own
  registry, so the package directory has a src-xxx hash suffix and is NOT
  the canonical tool-mklittlefs directory. PlatformIO's SCons builder calls
  mklittlefs as a bare command name, relying on PATH. If the versioned
  package dir is not on PATH, buildfs fails with "not recognized". We prepend
  the directory that actually contains mklittlefs.exe so buildfs finds it.
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
# Applies to every ESP32-S3 env on the pioarduino platform: esp32, esp32-classic,
# and esp32-combo, which extend esp32 and need the same mklittlefs on PATH for
# `-t buildfs`. startswith("esp32") matches all three.
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
