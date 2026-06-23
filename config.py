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
# Per-build artifact archive: every build's artifacts are compressed into a
# single <semver>.zip so the exact .elf (for addr2line crash decode) survives
# later builds. Lives OUTSIDE the repo (sibling ../OTGW-build-archive) so the
# working tree stays clean and the archive is shared across worktrees. Override
# with OTGW_ARCHIVE_DIR (absolute path wins; relative is taken under the parent).
ARCHIVE_DIR = PROJECT_DIR.parent / os.getenv("OTGW_ARCHIVE_DIR", "OTGW-build-archive")
