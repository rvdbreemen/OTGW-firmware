#!/usr/bin/env python3
"""TASK-1857 / TASK-1903 item 1 host harness: a loopback or replayed MsgID 3 is no boiler evidence.

print_slavememberid() in OTGW-Core.ino decodes every MsgID 3 frame. An OT-Direct loopback answer
(OTdata.bLocalAnswer) or a line from the /otgw_simulation.log replay (OTdata.bReplayed) must not
feed satNoteSlaveConfig() (the SAT PWM permission), state.sat.iDetectedHeatingSource or
satDetectManufacturer(); a real boiler Read-Ack must still feed all three.

    python test/host/test_slave_config_evidence.py            # working tree
    python test/host/test_slave_config_evidence.py --rev HEAD # any git revision (old code)

Slices the REAL function from src/OTGW-firmware/OTGW-Core.ino and compiles it with MSVC.
Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, SOURCE, find_vcvars, read_source
from test_replay_frame_guards import slice_block


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rev")
    args = ap.parse_args()
    origin = f"git {args.rev}" if args.rev else "working tree"
    lines = read_source(args.rev).splitlines()
    GEN.mkdir(exist_ok=True)
    hdr = f"// GENERATED from {SOURCE} ({origin}) by test_slave_config_evidence.py. Do not edit.\n"
    body = slice_block(lines, r"^void print_slavememberid\(uint16_t& value\)")
    (GEN / "slave_config_evidence.inc").write_text(hdr + body + "\n", encoding="utf-8")
    print(f"== sliced print_slavememberid() ({origin})")
    exe = GEN / "test_slave_config_evidence.exe"
    if exe.exists():
        exe.unlink()
    bat = GEN / "_compile_slave_config_evidence.bat"
    gen_fwd = str(GEN).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{gen_fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_slave_config_evidence.cpp'}\" /I\"{HERE}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-3000:], c.stderr[-2000:])
        print("COMPILATION FAILED")
        return 2
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    print(r.stdout, end="")
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
