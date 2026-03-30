"""
PlatformIO pre-build script: update version.h via autoinc-semver.py.

Runs automatically when PlatformIO builds (extra_scripts = pre:scripts/platformio_version.py).
Also works standalone when called from build.py.

Note: PlatformIO executes extra_scripts via exec(), so __file__ is not available.
We use the PlatformIO env to resolve the project directory instead.
"""

import subprocess
import sys
from pathlib import Path


def _resolve_paths(project_dir=None):
    """Resolve project paths. Uses project_dir if given, else tries __file__."""
    if project_dir is not None:
        pdir = Path(project_dir)
    else:
        # Standalone invocation
        pdir = Path(__file__).parent.parent.resolve()
    scripts_dir = pdir / "scripts"
    firmware_dir = pdir / "src" / "OTGW-firmware"
    semver_script = scripts_dir / "autoinc-semver.py"
    return pdir, firmware_dir, semver_script


def get_git_hash(project_dir):
    """Get short git hash, or 'local' if unavailable."""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short=7", "HEAD"],
            cwd=project_dir,
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except (FileNotFoundError, OSError):
        pass
    return "local"


def update_version(project_dir=None):
    """Run autoinc-semver.py to update version.h."""
    pdir, firmware_dir, semver_script = _resolve_paths(project_dir)

    if not semver_script.exists():
        print(f"[version] WARNING: {semver_script} not found, skipping version update")
        return

    githash = get_git_hash(pdir)
    cmd = [
        sys.executable,
        str(semver_script),
        str(firmware_dir),
        "--filename", "version.h",
        "--githash", githash,
    ]
    print(f"[version] Updating version.h (hash={githash})")
    subprocess.run(cmd, check=True)


# When executed by PlatformIO, Import is injected into the global scope.
# __file__ is NOT available (PlatformIO uses exec()), so we get the project
# directory from the PlatformIO environment.
try:
    Import("env")  # noqa: F821 — PlatformIO injects this
    _pio_project_dir = env.subst("$PROJECT_DIR")  # noqa: F821
    update_version(_pio_project_dir)
except (ImportError, NameError):
    # Not running inside PlatformIO — allow standalone use
    if __name__ == "__main__":
        update_version()
