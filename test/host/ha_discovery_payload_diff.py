#!/usr/bin/env python3
"""TASK-2971.1/.2/.3: compare the Home Assistant discovery payload set of two revisions with the real composers.

Builds the code under test twice with the host harness of test_ha_discovery_json.py (OLD = a git revision, default
14539282e (the R1 release commit, before H1); NEW = the working tree), runs every public composer, and compares what would be published:

  - the set of discovery config topics must be identical (no entity added or removed);
  - for every config topic the uniq_id, name, stat_t and cmd_t must be identical (HA keeps its entity ids);
  - the keys that may differ are only the ones the HA tidy changes: dev_cla, entity_category, enabled_by_default,
    avty_t, exp_aft (device_class is dev_cla spelled out). Any other changed key fails the run.

Usage: python test/host/ha_discovery_payload_diff.py [--old-rev 14539282e]
Exit 0 = the two sets are the same entities with only the allowed keys changed.
"""
import argparse
import json
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_ha_discovery_json as H  # noqa: E402

ALLOWED = {"dev_cla", "device_class", "entity_category", "enabled_by_default", "avty_t", "exp_aft"}
IDENTITY = ("uniq_id", "name", "stat_t", "cmd_t")


def configs(run):
    out = {}
    for p in run[2]:
        if not p["raw"] or not p["topic"].endswith("/config"):
            continue
        try:
            out[p["topic"]] = json.loads(p["raw"].decode("utf-8"))
        except ValueError:
            out[p["topic"]] = {"__unparsable__": p["raw"].decode("utf-8", "replace")}
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--old-rev", default="14539282e")   # the R1 release commit, the last revision before H1; origin/dev carries H1 once T2 lands. Works only while that commit stays reachable.
    args = ap.parse_args()
    root = H.GEN / "ha_discovery_payload_diff"
    H.export(args.old_rev, root / "old")
    old = configs(H.build_and_run(f"OLD ({args.old_rev})", root / "old", args.old_rev))
    H.export(None, root / "new")
    new = configs(H.build_and_run("NEW (working tree)", root / "new"))

    problems = []
    added, removed = sorted(set(new) - set(old)), sorted(set(old) - set(new))
    if added:
        problems.append(f"{len(added)} config topics added, e.g. {added[:3]}")
    if removed:
        problems.append(f"{len(removed)} config topics removed, e.g. {removed[:3]}")
    changed = Counter()
    for topic in sorted(set(old) & set(new)):
        o, n = old[topic], new[topic]
        for k in IDENTITY:
            if o.get(k) != n.get(k):
                problems.append(f"{topic}: {k} changed {o.get(k)!r} -> {n.get(k)!r}")
        for k in sorted(set(o) | set(n)):
            if o.get(k) != n.get(k):
                if k in ALLOWED:
                    changed[k] += 1
                elif k not in IDENTITY:
                    problems.append(f"{topic}: unexpected key {k} changed {o.get(k)!r} -> {n.get(k)!r}")
    print(f"config topics: old {len(old)}, new {len(new)}, common {len(set(old) & set(new))}")
    print("payloads with a changed allowed key: " + (", ".join(f"{k} x{v}" for k, v in sorted(changed.items())) or "none"))
    if problems:
        print(f"FAIL: {len(problems)} problems")
        for p in problems[:20]:
            print("  " + p)
        return 1
    print("OK: same entities (topics, uniq_id, name, stat_t, cmd_t), only allowed keys changed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
