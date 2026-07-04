#!/usr/bin/env python3
"""adr_governance.py - repo-local ADR governance checks for OTGW-firmware.

A self-contained realization of the adr-kit governance plan
(docs/plan/adr-kit-governance-plan.md), pending the adr-kit CLI (TASK-422/424).
No third-party dependencies; operates directly on docs/adr/*.md and the index.

Subcommands:
  lint         Structural checks (superseded_by resolves; ADR-080 gate exists;
               status <-> superseded_by reciprocity). Exit 1 on any FAIL.
  index-check  Assert every ADR is listed exactly once in docs/adr/README.md.
               Exit 1 on a missing or duplicated entry.
  doctor       Human-readable health report (status histogram, Proposed list,
               index completeness, drift flags). Exit 0 unless --strict.

Flags:
  --strict     Promote WARN to FAIL (non-zero exit).
  --json       Machine-readable output.
"""
import argparse
import glob
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ADR_DIR = os.path.join(REPO, "docs", "adr")
README = os.path.join(ADR_DIR, "README.md")
EVALUATE = os.path.join(REPO, "evaluate.py")

STATUS_KEYWORDS = [
    ("Superseded", ("supersed",)),
    ("Rejected", ("reject",)),
    ("Deprecated", ("deprecat",)),
    ("Deferred", ("defer",)),
    ("Amended", ("amend",)),
    ("Accepted", ("accept",)),
    ("Proposed", ("propos",)),
]


def status_category(text):
    low = (text or "").lower()
    for cat, keys in STATUS_KEYWORDS:
        if any(k in low for k in keys):
            return cat
    return "Unknown"


def _fm_list(fm, key):
    m = re.search(r"^%s:\s*\[(.*?)\]" % re.escape(key), fm, re.M)
    if not m:
        return []
    return sorted({int(x) for x in re.findall(r"ADR-(\d+)", m.group(1))})


def parse_adr(path):
    text = open(path, encoding="utf-8").read()
    num = int(re.search(r"ADR-(\d+)", os.path.basename(path)).group(1))
    fm_match = re.match(r"^---\n(.*?)\n---\n", text, re.S)
    fm = fm_match.group(1) if fm_match else ""

    status_line = ""
    fm_status = re.search(r"^status:\s*(.+)$", fm, re.M)
    if fm_status:
        status_line = fm_status.group(1).strip().strip('"')
    # human ## Status section: first non-empty line
    sec = re.search(r"^##\s*Status\s*\n+(.+)", text, re.M)
    prose_status = sec.group(1).strip() if sec else ""
    # older ADRs use an inline "**Status:** X" line instead of a ## Status section
    inline = re.search(r"^\*\*Status\*?\*?:?\*{0,2}\s*(.+)$", text, re.M)
    inline_status = inline.group(1).strip() if inline else ""
    effective_status = status_line or prose_status or inline_status

    superseded_by = _fm_list(fm, "superseded_by")
    supersedes = _fm_list(fm, "supersedes")
    # fall back to prose if frontmatter is absent/empty
    scope = (status_line + " " + prose_status + " " + inline_status)
    if not superseded_by:
        superseded_by = sorted({int(x) for x in re.findall(r"[Ss]upersed\w*\s+by\s+\[?ADR-(\d+)", scope)})

    # ADR-080: a "Binding" ADR that names a check_* gate must have that gate.
    named_gates = sorted(set(re.findall(r"\b(check_[a-z0-9_]+)\b", text)))
    is_binding = bool(re.search(r"\bBinding\b", text)) and not re.search(r"guideline-level", text, re.I)

    return {
        "num": num,
        "path": os.path.relpath(path, REPO),
        "title": (re.search(r"^title:\s*(.+)$", fm, re.M) or [None, os.path.basename(path)])[1]
        if fm else os.path.basename(path),
        "status_raw": effective_status,
        "status": status_category(effective_status),
        "supersedes": supersedes,
        "superseded_by": superseded_by,
        "named_gates": named_gates,
        "is_binding": is_binding,
        "has_frontmatter": bool(fm),
    }


def load_all():
    adrs = {}
    for p in sorted(glob.glob(os.path.join(ADR_DIR, "ADR-*.md"))):
        a = parse_adr(p)
        adrs[a["num"]] = a
    return adrs


def evaluate_gate_names():
    if not os.path.exists(EVALUATE):
        return set()
    txt = open(EVALUATE, encoding="utf-8").read()
    names = set(re.findall(r"^\s*def\s+(check_[a-z0-9_]+)", txt, re.M))
    # also tests/
    for tp in glob.glob(os.path.join(REPO, "tests", "*.py")):
        names |= set(re.findall(r"\b(check_[a-z0-9_]+)\b", open(tp, encoding="utf-8").read()))
    return names


def readme_entries(readme_path=None):
    """Return dict: adr_num -> count of link occurrences in the index."""
    txt = open(readme_path or README, encoding="utf-8").read()
    counts = {}
    for n in re.findall(r"\]\(ADR-(\d+)[^)]*\.md\)", txt):
        counts[int(n)] = counts.get(int(n), 0) + 1
    return counts


# ---------------------------------------------------------------- lint
def cmd_lint(adrs, gates):
    """Pure: returns (fails, warns). `gates` is the set of known check_* names."""
    fails, warns = [], []
    for n, a in adrs.items():
        for tgt in a["superseded_by"]:
            if tgt not in adrs:
                fails.append(f"ADR-{n}: superseded_by ADR-{tgt} which does not exist")
        # status must agree with superseded_by
        if a["superseded_by"] and a["status"] not in ("Superseded", "Amended"):
            warns.append(f"ADR-{n}: has superseded_by but status is '{a['status']}' (expected Superseded)")
        # reciprocity: if A supersedes B (frontmatter), B should point back
        for tgt in a["supersedes"]:
            if tgt in adrs and n not in adrs[tgt]["superseded_by"]:
                warns.append(f"ADR-{n}: supersedes ADR-{tgt} but ADR-{tgt}.superseded_by does not list {n}")
        # ADR-080: a Binding ADR that names a gate must have that gate present
        if a["is_binding"] and a["status"] == "Accepted":
            missing = [g for g in a["named_gates"] if g not in gates]
            if a["named_gates"] and missing:
                fails.append(f"ADR-{n}: binding+Accepted names gate(s) {missing} absent from evaluate.py/tests")
        if a["status"] == "Unknown":
            warns.append(f"ADR-{n}: could not resolve a status")
    return fails, warns


# ---------------------------------------------------------------- index-check
def cmd_index_check(adrs, counts):
    """Pure: `counts` maps adr_num -> occurrences in the index."""
    fails = []
    for n in sorted(adrs):
        c = counts.get(n, 0)
        if c == 0:
            fails.append(f"ADR-{n} ({adrs[n]['status']}) is MISSING from docs/adr/README.md")
        elif c > 1:
            fails.append(f"ADR-{n} appears {c} times in docs/adr/README.md (duplicate)")
    return fails


# ---------------------------------------------------------------- doctor
def cmd_doctor(adrs):
    hist = {}
    for a in adrs.values():
        hist[a["status"]] = hist.get(a["status"], 0) + 1
    proposed = sorted(n for n, a in adrs.items() if a["status"] == "Proposed")
    counts = readme_entries()
    missing = sorted(n for n in adrs if counts.get(n, 0) == 0)
    dups = sorted(n for n in adrs if counts.get(n, 0) > 1)
    no_fm = sorted(n for n, a in adrs.items() if not a["has_frontmatter"])
    return {
        "total": len(adrs),
        "status_histogram": hist,
        "proposed": proposed,
        "index_missing": missing,
        "index_duplicates": dups,
        "no_frontmatter": no_fm,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["lint", "index-check", "doctor"])
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--adr-dir", default=None, help="override docs/adr (for testing)")
    args = ap.parse_args()

    global ADR_DIR, README
    if args.adr_dir:
        ADR_DIR = args.adr_dir
        README = os.path.join(ADR_DIR, "README.md")

    adrs = load_all()
    rc = 0

    if args.command == "lint":
        fails, warns = cmd_lint(adrs, evaluate_gate_names())
        if args.json:
            print(json.dumps({"fail": fails, "warn": warns}, indent=2))
        else:
            for f in fails:
                print("FAIL " + f)
            for w in warns:
                print("WARN " + w)
            print(f"\nlint: {len(fails)} fail, {len(warns)} warn over {len(adrs)} ADRs")
        rc = 1 if fails or (args.strict and warns) else 0

    elif args.command == "index-check":
        fails = cmd_index_check(adrs, readme_entries())
        if args.json:
            print(json.dumps({"fail": fails}, indent=2))
        else:
            for f in fails:
                print("FAIL " + f)
            print(f"\nindex-check: {len(fails)} fail over {len(adrs)} ADRs")
        rc = 1 if fails else 0

    elif args.command == "doctor":
        rep = cmd_doctor(adrs)
        if args.json:
            print(json.dumps(rep, indent=2))
        else:
            print(f"ADR doctor: {rep['total']} ADRs")
            print("  status:      " + ", ".join(f"{k}={v}" for k, v in sorted(rep["status_histogram"].items())))
            print(f"  proposed:    {len(rep['proposed'])} -> {rep['proposed']}")
            print(f"  index missing:    {rep['index_missing'] or 'none'}")
            print(f"  index duplicates: {rep['index_duplicates'] or 'none'}")
            print(f"  no frontmatter:   {len(rep['no_frontmatter'])} ADRs (migration candidates)")
        rc = 1 if args.strict and (rep["index_missing"] or rep["index_duplicates"]) else 0

    sys.exit(rc)


if __name__ == "__main__":
    main()
