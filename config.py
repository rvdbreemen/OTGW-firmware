import os
from pathlib import Path

# Base Paths
PROJECT_DIR = Path(__file__).parent.resolve()

# Structural Config (Fixed)
PROJECT_NAME = "OTGW-firmware"
FIRMWARE_ROOT = PROJECT_DIR / "src" / "OTGW-firmware"
DATA_DIR = FIRMWARE_ROOT / "data"

# Environment Config (Overridable)
# Allows overriding build directory via environment variable
BUILD_DIR = PROJECT_DIR / os.getenv("OTGW_BUILD_DIR", "build")
TEMP_DIR = PROJECT_DIR / ".tmp"
# Per-build artifact archive: persistent, never wiped (unlike BUILD_DIR). Each
# build copies its .bin/.elf/.zip into build-archive/<semver>/ so the exact
# artifacts (notably the .elf for addr2line crash decode) survive later builds.
ARCHIVE_DIR = PROJECT_DIR / os.getenv("OTGW_ARCHIVE_DIR", "build-archive")
