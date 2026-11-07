#!/usr/bin/env python3
"""TASK-1654: export the TASK-1412.20 wire scenarios as boiler-simulator scenario files (.scn).

The phases come from scripts/sat_bench_scenarios_1412_20.py (SCENARIOS, DEFAULT_STATE), imported and
never changed: a change there raises the generator VERSION and the fixture sha256 values in run
plans (docs/sat/bench-simulator-1412-20.md). This exporter only translates.

A replay fixture and a wire boiler differ in who decides what (docs/sat/bench-simulator-1412-20.md
section 2). The translation therefore keeps the boiler's side and drops the rest:
  kept:     duration, reply (per-ID Data-Invalid / Unknown-DataID / no answer), silent, flow, ret,
            pressure, outside, member_id, slave_hb, fault
  dropped:  ch_enable, dhw_enable, tset (the master on the wire, SAT, decides these) and
            ch, flame, mod, dhw (the replay scripts the boiler status; the simulator derives it
            from the master's request, see src/boiler_sim.h)
  added:    WIRE_OVERRIDES below, from each scenario's expected_wire (SB11: flame 120 s after CH=1),
            and a final healthy phase that holds after the scripted timeline.

Output: tools/bench/ot-boiler-sim/data/<id>.scn plus data/index.txt (generator version, sha256 per
file). Standard library only, deterministic. tests in test/host/test_ot_boiler_sim.py check that the
committed files equal a fresh export.

Usage: python tools/bench/ot-boiler-sim/export_scenarios.py [--out DIR] [--check]
"""
import argparse
import hashlib
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent.parent
for cand in (REPO / "scripts", REPO / "tools" / "research" / "sat"):
    if (cand / "sat_bench_scenarios_1412_20.py").exists():
        sys.path.insert(0, str(cand))
        break
import sat_bench_scenarios_1412_20 as gen  # noqa: E402

EXPORT_IDS = ("SB05-id25-data-invalid", "SB06-id25-unknown", "SB07-id25-no-answer",
              "SB08-id28-data-invalid", "SB09-id28-unknown", "SB11-slow-ignition")

BOILER_KEYS = ("flow", "ret", "pressure", "outside", "member_id", "slave_hb", "fault")
MASTER_OR_REPLAY_KEYS = ("ch_enable", "dhw_enable", "tset", "ch", "flame", "mod", "dhw")

# Wire defaults the replay does not carry. mod: the modulation the boiler reports while the flame
# burns (the replay scripts mod per phase together with the flame).
WIRE_DEFAULTS = {"mod": 30, "ignition_delay": 0}

# Per scenario, from its expected_wire in the generator.
WIRE_OVERRIDES = {
    # expected_wire: "reactive slave: flame 120 s after CH=1" (entry 80; test_d3/test_d8)
    "SB11-slow-ignition": {"ignition_delay": 120},
}

REPLY_NAMES = {"ack": "ack", "data_invalid": "data_invalid", "unknown": "unknown", "none": "none"}


def fmt(v):
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, float):
        return ("%.2f" % v).rstrip("0").rstrip(".")
    return str(v)


def phase_tokens(ph, overrides):
    toks = []
    for k, v in ph.items():
        if k == "duration" or k in MASTER_OR_REPLAY_KEYS:
            continue
        if k == "reply":
            for mid in sorted(v):
                toks.append("reply%d=%s" % (mid, REPLY_NAMES[v[mid]]))
        elif k == "silent":
            toks.append("silent=%s" % fmt(v))
        elif k in BOILER_KEYS:
            toks.append("%s=%s" % (k, fmt(v)))
        else:
            raise SystemExit("generator phase key %r has no simulator mapping; extend export_scenarios.py" % k)
    for k in sorted(overrides):
        toks.append("%s=%s" % (k, fmt(overrides[k])))
    return toks


def export_one(sc):
    d = gen.DEFAULT_STATE
    wire = dict(WIRE_DEFAULTS)
    wire.update(WIRE_OVERRIDES.get(sc["id"], {}))
    lines = [
        "# OpenTherm boiler simulator scenario, TASK-1654. Exported from %s by export_scenarios.py."
        % "sat_bench_scenarios_1412_20.py",
        "# %s" % sc["title"],
        "scenario %s" % sc["id"],
        "source sat_bench_scenarios_1412_20 %s" % gen.VERSION,
        "default " + " ".join("%s=%s" % (k, fmt(d[k])) for k in BOILER_KEYS)
        + " " + " ".join("%s=%s" % (k, fmt(v)) for k, v in sorted(wire.items())),
    ]
    phases = sc["phases"]
    for ph in phases:
        toks = phase_tokens(ph, {})
        lines.append(("phase %d %s" % (int(ph["duration"]), " ".join(toks))).rstrip())
    # After the generator's timeline the boiler is healthy again and stays so (the last phase holds).
    # This lets a wire run see the recovery the expected_wire outputs name (a later Read-Ack retracts
    # an Unknown-DataID, a valid flow follows a fault window).
    lines.append("phase 0  # healthy hold after the scripted timeline")
    return "\n".join(lines) + "\n"


def export(out: Path):
    out.mkdir(parents=True, exist_ok=True)
    by_id = {sc["id"]: sc for sc in gen.SCENARIOS}
    index = ["# generator sat_bench_scenarios_1412_20 %s; sha256 per scenario file" % gen.VERSION]
    files = {}
    for sid in EXPORT_IDS:
        text = export_one(by_id[sid])
        name = "%s.scn" % sid
        files[name] = text
        index.append("%s %s" % (hashlib.sha256(text.encode("ascii")).hexdigest(), name))
    files["index.txt"] = "\n".join(index) + "\n"
    return files


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--out", default=str(HERE / "data"))
    p.add_argument("--check", action="store_true", help="exit 1 when the files in --out differ from a fresh export")
    a = p.parse_args(argv)
    out = Path(a.out)
    files = export(out)
    stale = []
    for name, text in files.items():
        path = out / name
        if a.check:
            if not path.exists() or path.read_bytes() != text.encode("ascii"):
                stale.append(name)
        else:
            path.write_bytes(text.encode("ascii"))
    if a.check:
        print("stale: %s" % ", ".join(stale) if stale else "up to date")
        return 1 if stale else 0
    print("wrote %d files to %s (generator %s)" % (len(files), out, gen.VERSION))
    return 0


if __name__ == "__main__":
    sys.exit(main())
