#!/usr/bin/env python3
"""TASK-1736 AC2 host harness: the real AsyncResponseStream write path on the real cbuf.

    python test/host/test_stream_real_cbuf.py

Runs build_and_run_stream_real_cbuf.ps1 twice:
  1. as shipped: the presize the firmware chooses must give one ring buffer per
     response and no temporary copies (PASS expected);
  2. with OTGW_NO_PRESIZE=1, the old 1460 B start: the same checks must FAIL, which
     shows the test still bites on the staircase.
Exit code: 0 when run 1 passes and run 2 fails (or both SKIP without a firmware build),
1 otherwise, 2 harness error.
"""
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PS1 = HERE / "build_and_run_stream_real_cbuf.ps1"
EXE = HERE / "generated" / "test_stream_real_cbuf.exe"


def main():
    r = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(PS1)],
                       capture_output=True, text=True, errors="replace")
    print(r.stdout[-4000:], r.stderr[-2000:], sep="")
    if "SKIP:" in r.stdout:
        return 0
    if r.returncode == 2 or not EXE.exists():
        print("HARNESS ERROR")
        return 2
    if r.returncode != 0:
        print("FAIL: the presize path does not pass")
        return 1
    env = dict(os.environ, OTGW_NO_PRESIZE="1")
    red = subprocess.run([str(EXE)], capture_output=True, text=True, env=env, errors="replace")
    print("== presize disabled (must fail) ==")
    print(red.stdout[-2500:], end="")
    if red.returncode == 0:
        print("FAIL: with the presize disabled the test still passes, so it does not detect the staircase")
        return 1
    print("PASS: green with the presize, red without it")
    return 0


if __name__ == "__main__":
    sys.exit(main())
