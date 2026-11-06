"""
PlatformIO pre-build script (TASK-1640): gzip the firmware update pages
(src/OTGW-firmware/webui/*.html) into src/OTGW-firmware/updateServerHtml.h.

Runs automatically when PlatformIO builds (extra_scripts = pre:scripts/platformio_webui.py).
PlatformIO executes extra_scripts via exec(), so __file__ is not available; the
project directory comes from the PlatformIO env.
"""
import sys
from pathlib import Path

Import("env")  # noqa: F821  (provided by PlatformIO)

_project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(_project / "scripts"))
import webui_pages  # noqa: E402

if webui_pages.write_update_header(_project):
    print("webui: regenerated updateServerHtml.h")
