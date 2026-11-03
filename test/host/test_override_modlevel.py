#!/usr/bin/env python3
"""TASK-1389 host harness: the <label>/override topic of MaxRelModLevelSetting (MsgID 14) through the real code.

    python test/host/test_override_modlevel.py                    # FIX (working tree)
    python test/host/test_override_modlevel.py --old-rev HEAD     # plus OLD, a git revision

gruijter's alpha.504 capture: MaxRelModLevelSetting/override published 0.00 every minute, also after
MM=100. The PIC then sends T (thermostat write), R (the write the PIC substituted, MM=100), B (boiler
Write-Ack) and A (the PIC's Write-Ack to the thermostat). print_f88() recorded the T value (the
thermostat's original) and the A frame, whose data byte is per spec undefined for a Write-Ack the slave
does not echo (bSlaveEchoesValue = false): the last capture, the A, left 0.00 in the store.

The code under test is sliced by anchor from OTGW-Core.ino (print_f88(), the validity gates,
recordOTOverride(), publishActiveOverrides(), the f8.8 codec) and OTGW-Core.h (types, store); only the
platform and the edges (log, MQTT send) are doubled. Frames reach print_f88() with the flags processOT()
sets for the pairings (T,R) and (B,A). OLD must fail O1 and O2 and pass the controls; FIX passes all.
Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import re
import sys

from test_dhw_water_meter import CORE, CORE_H, FW_H, HW_H, build_and_run, construct, read, typedef_ending, write
from test_ot_reserved_range import ANCHORS as RESERVED_ANCHORS
from test_ot_reserved_range import GEN, slice_by_anchor

CPP = "test_override_modlevel.cpp"
CASES = ["O1", "O2", "O3", "O4"]
OLD_DEFECT = ["O1", "O2"]


def export(rev, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    ch, hw = read(CORE_H, rev).splitlines(), read(HW_H, rev).splitlines()
    fwh, core = read(FW_H, rev).splitlines(), read(CORE, rev).splitlines()
    origin = f"git {rev}" if rev else "the working tree"
    write(out_dir, "gen_types.inc", origin, [
        construct(hw, r"^enum OTGWHardwareMode\b"),
        typedef_ending(ch, "OTdataStruct"),
        construct(ch, r"^enum OTLibMessageType\b"),
        construct(ch, r"^enum OTLibMessageID\b"),
        construct(ch, r"^\s*enum OTtype_t\b"),
        construct(ch, r"^\s*enum OTmsgcmd_t\b"),
        construct(ch, r"^\s*struct OTlookup_t\b"),
        construct(ch, r"^enum OTGW_response_type\b"),
        construct(ch, r"^struct OpenthermData_t\b"),
        construct(ch, r"^enum OTOverrideKind\b"),
        construct(ch, r"^struct OTOverrideEntry_t\b"),
        "\n".join(l for l in ch if re.match(r"^#define OVERRIDE_(STORE_MAX|ACTIVE_TIMEOUT)\b", l)),
    ])
    write(out_dir, "gen_fw_h.inc", origin, [
        construct(fwh, r"^inline bool isOTDirectEnabled\(\)"),
        construct(fwh, r"^inline bool isFlashing\(\)"),
    ])
    write(out_dir, "gen_core.inc", origin, [
        construct(core, r"^float OpenthermData_t::f88\(\)"),
        construct(core, r"^void OpenthermData_t::f88\(float value\)"),
        *[slice_by_anchor(core, a) for a in RESERVED_ANCHORS],
        construct(core, r"^bool is_value_valid\(OpenthermData_t OT, OTlookup_t OTlookup\)\s*\{"),
        construct(core, r"^bool is_value_valid_for_master_topic\(OpenthermData_t OT, OTlookup_t OTlookup\)\s*\{"),
        construct(core, r"^void recordOTOverride\(uint8_t id, uint8_t kind, float value\)\s*\{"),
        construct(core, r"^void publishActiveOverrides\(\)\s*\{"),
        construct(core, r"^void print_f88\(float& value\)\s*$"),
    ])


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--old-rev", help="also build the code under test from this git revision (OLD)")
    args = ap.parse_args()
    root = GEN / "override_modlevel"
    checks = []

    export(None, root / "fix")
    rc, out, cases = build_and_run("FIX (working tree)", root / "fix", CPP)
    print("== FIX (working tree) ==")
    print(out, end="")
    checks.append(("FIX passes every case", rc == 0 and sorted(cases) == sorted(CASES) and all(v == "pass" for v in cases.values())))

    if args.old_rev:
        export(args.old_rev, root / "old")
        orc, oout, ocases = build_and_run(f"OLD (git {args.old_rev})", root / "old", CPP)
        print(f"== OLD (OTGW-Core.ino and OTGW-Core.h at git {args.old_rev}) ==")
        print(oout, end="")
        failed = sorted(k for k, v in ocases.items() if v == "FAIL")
        checks.append((f"OLD fails exactly {','.join(OLD_DEFECT)} (failed: {','.join(failed) or 'none'})", failed == sorted(OLD_DEFECT)))
        checks.append(("OLD passes the controls", all(ocases.get(k) == "pass" for k in CASES if k not in OLD_DEFECT)))

    print("== verdict ==")
    for text, ok in checks:
        print(f"  {text}: {'yes' if ok else 'NO'}")
    ok = all(ok for _, ok in checks)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
