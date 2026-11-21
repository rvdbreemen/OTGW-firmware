#!/usr/bin/env python3
"""TASK-1649 / TASK-1650 host harness: replayed MsgID 27 must not stamp the outside temperature;
a banner on OT-Direct must not call satNotifyPicRestart(); a replayed banner must not re-enable the PIC
or persist a board mode (TASK-1902); a replayed T MsgID 16 must not reach SAT (TASK-1901).

    python test/host/test_replay_frame_guards.py            # working tree
    python test/host/test_replay_frame_guards.py --rev HEAD # any git revision (old code)

Slices the REAL blocks from processOT() in src/OTGW-firmware/OTGW-Core.ino and compiles them with MSVC.
Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, SOURCE, host_cxx_build, read_source


def slice_block(lines, anchor_rx, brace_from_anchor_line=True):
    start = next((i for i, l in enumerate(lines) if re.search(anchor_rx, l)), None)
    if start is None:
        raise SystemExit(f"anchor not found: {anchor_rx}")
    depth, out, opened = 0, [], False
    for line in lines[start:]:
        out.append(line)
        depth += line.count("{") - line.count("}")
        if "{" in line:
            opened = True
        if opened and depth == 0:
            return "\n".join(out)
    raise SystemExit("unterminated block")


def slice_banner(lines):
    start = next((i for i, l in enumerate(lines) if "strstr(buf, OTGW_BANNER)" in l), None)
    if start is None:
        raise SystemExit("anchor not found: strstr(buf, OTGW_BANNER)")
    end = next(i for i in range(start, len(lines)) if re.search(r"^\s*if \(!state\.pic\.bAvailable\b", lines[i]))
    return "\n".join(lines[start + 1:end])


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rev")
    args = ap.parse_args()
    origin = f"git {args.rev}" if args.rev else "working tree"
    lines = read_source(args.rev).splitlines()
    GEN.mkdir(exist_ok=True)
    hdr = f"// GENERATED from {SOURCE} ({origin}) by test_replay_frame_guards.py. Do not edit.\n"
    # TASK-1896 moved the frame rule into otFrameIsFreshOutsideTemp(); an older revision has it inline.
    if any(re.search(r"^static bool otFrameIsFreshOutsideTemp\(", l) for l in lines):
        pred = slice_block(lines, r"^static bool otFrameIsFreshOutsideTemp\(")
        stamp = slice_block(lines, r"^\s*if \(otFrameIsFreshOutsideTemp\(OTdata\)\)")
    else:
        pred = "// (no otFrameIsFreshOutsideTemp() in this revision)"
        stamp = slice_block(lines, r"^\s*if \(OTdata\.id == 27 &&")
    (GEN / "replay_guard_outside27_pred.inc").write_text(hdr + pred + "\n", encoding="utf-8")
    (GEN / "replay_guard_outside27.inc").write_text(hdr + stamp + "\n", encoding="utf-8")
    (GEN / "replay_guard_banner.inc").write_text(hdr + slice_banner(lines) + "\n", encoding="utf-8")
    # TASK-1902: the PIC-recovery block right after it (sets state.pic.bAvailable and persists iBoardMode).
    # Searched from the processOT() banner branch on: applyPICBannerInfo() has an earlier block of the same shape.
    b0 = next(i for i, l in enumerate(lines) if "strstr(buf, OTGW_BANNER)" in l)
    recovery = slice_block(lines[b0:], r"^\s*if \(!state\.pic\.bAvailable\b")
    (GEN / "replay_guard_banner_recovery.inc").write_text(hdr + recovery + "\n", encoding="utf-8")
    # TASK-1901: the thermostat MsgID 16 feed into satOnThermostatSetpoint().
    trset = slice_block(lines, r"^\s*if \(OTdata\.id == OT_TrSet && OTdata\.rsptype == OTGW_THERMOSTAT")
    (GEN / "replay_guard_trset.inc").write_text(hdr + trset + "\n", encoding="utf-8")
    print(f"== sliced the processOT() MsgID 27 stamp, banner, PIC-recovery and MsgID 16 blocks ({origin})")
    exe = GEN / "test_replay_frame_guards.exe"
    if exe.exists():
        exe.unlink()
    ok, out_ = host_cxx_build([HERE / 'test_replay_frame_guards.cpp'], exe, includes=[HERE], workdir=GEN)
    if not ok:
        print(out_[-3000:])
        print("COMPILATION FAILED")
        return 2
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    print(r.stdout, end="")
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
