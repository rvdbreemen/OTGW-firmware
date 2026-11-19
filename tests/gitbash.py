"""Find the bash that runs the project's POSIX scripts in tests (Git for Windows on Windows, never WSL).

On Windows `shutil.which("bash")` resolves to C:\\Windows\\System32\\bash.exe (the WSL launcher) whenever
System32 comes before Git on PATH, as in a PowerShell or cmd session. WSL bash eats the backslashes of a
Windows script path ("/bin/bash: D:UsersRobert...: No such file or directory", exit 127) and is not the
shell the users of these scripts run. Pass script paths as Path.as_posix() as well.
"""
import os
import shutil
from pathlib import Path


def _is_wsl_launcher(path) -> bool:
    p = str(path).lower().replace("/", "\\")
    return "\\windows\\system32\\" in p or "\\windowsapps\\" in p


def find_bash():
    """Git for Windows bash on Windows, else the bash on PATH; None when there is none."""
    cands = []
    if os.name == "nt":
        git = shutil.which("git")
        if git:
            g = Path(git).resolve()
            for root in g.parents:   # ...\Git\cmd\git.exe, ...\Git\mingw64\bin\git.exe, ...\Git\bin\git.exe
                if (root / "bin" / "bash.exe").exists() and (root / "usr" / "bin").is_dir():
                    cands.append(root / "bin" / "bash.exe")
                    break
        cands += [Path(r"C:\Program Files\Git\bin\bash.exe"), Path(r"C:\Program Files\Git\usr\bin\bash.exe")]
    which = shutil.which("bash")
    if which:
        cands.append(Path(which))
    for c in cands:
        if c.exists() and not (os.name == "nt" and _is_wsl_launcher(c)):
            return str(c)
    return None
