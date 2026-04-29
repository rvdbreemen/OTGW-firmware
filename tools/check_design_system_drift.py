#!/usr/bin/env python3
"""
Design-System Class Drift Detector
===================================

Scans data/*.html and data/*.js for CSS classes that are referenced but never
defined in any data/*.css file. The opposite (defined but unused) is reported
separately for awareness.

Why this exists
---------------
The design-package handoff (TASK-435) replaced legacy index.css with the
token-driven components.css + ds-tokens.css combo. JS/HTML still emit markup
using legacy class names (.pictable, .devinforow, .otmonrow, etc.). Without
this check, a legacy class can quietly stop being styled — visually broken
pages, no error, no warning.

This is the "Vorm A" diagnostic, kept as the manual view of the permanent
evaluate.py gate (Vorm B / TASK-470, ADR-091).

Usage
-----
    python tools/check_design_system_drift.py
    python tools/check_design_system_drift.py --json     # machine-readable
    python tools/check_design_system_drift.py --reverse  # also list dead CSS

Conservative scope (no AST, just regex):
- HTML/JS: class="…" / class='…' inside source
- JS DOM:  classList.{add,remove,toggle,replace}('…'), .className = '…'
- CSS:     any .classname selector (compound selectors handled)

Stripped before scan: CSS /* … */ comments, JS /* … */ and // comments.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple

PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from evaluate import (  # noqa: E402
    DESIGN_SYSTEM_CLASS_ALLOWLIST,
    compute_drift,
    extract_class_definitions_from_css,
    extract_classes_from_html,
    extract_classes_from_js,
    scan_design_system_workspace,
)

DATA_DIR = PROJECT_ROOT / "src" / "OTGW-firmware" / "data"


# ----- Driver -----

def scan_workspace(data_dir: Path) -> Tuple[Dict[str, List[str]], Set[str], Dict[str, int]]:
    """Scan all HTML/JS/CSS in data_dir. Return (used, defined, used_counts)."""
    return scan_design_system_workspace(data_dir)


def format_human(
    used: Dict[str, List[str]],
    defined: Set[str],
    used_counts: Dict[str, int],
    show_dead: bool,
) -> str:
    out: List[str] = []
    out.append("=== Design-System Class Drift Report ===")
    out.append("")
    out.append(f"Defined classes (in *.css):     {len(defined)}")
    out.append(f"Referenced classes (in HTML/JS): {len(used)}")

    missing, dead, allowlisted = compute_drift(used, defined, DESIGN_SYSTEM_CLASS_ALLOWLIST)

    out.append(f"Drift - used but NOT defined:    {len(missing)}")
    out.append(f"Allowlisted no-style classes:    {len(allowlisted)}")
    if show_dead:
        out.append(f"Dead CSS - defined but UNUSED:   {len(dead)}")
    out.append("")

    if missing:
        out.append("--- Missing CSS rules (priority list for porting) ---")
        # Sort: highest usage count first, then alphabetic
        for cls in sorted(missing.keys(), key=lambda c: (-used_counts[c], c)):
            locs = missing[cls]
            sample = ", ".join(locs[:3])
            extra = f" (+{len(locs)-3} more)" if len(locs) > 3 else ""
            out.append(f"  {cls:<30s}  used {used_counts[cls]:>3}x   first: {sample}{extra}")
        out.append("")
    else:
        out.append("OK - no drift: every referenced class is defined.")
        out.append("")

    if show_dead:
        out.append("--- Dead CSS classes (defined but never used) ---")
        if dead:
            for cls in sorted(dead):
                out.append(f"  {cls}")
        else:
            out.append("  (none)")
        out.append("")

    return "\n".join(out)


def format_json(
    used: Dict[str, List[str]],
    defined: Set[str],
    used_counts: Dict[str, int],
    show_dead: bool,
) -> str:
    missing, dead, allowlisted = compute_drift(used, defined, DESIGN_SYSTEM_CLASS_ALLOWLIST)
    payload = {
        "summary": {
            "defined_count": len(defined),
            "used_count": len(used),
            "drift_count": len(missing),
            "allowlisted_count": len(allowlisted),
            "dead_count": len(dead) if show_dead else None,
        },
        "missing": {
            cls: {"count": used_counts[cls], "locations": locs}
            for cls, locs in missing.items()
        },
        "allowlisted": {
            cls: {"count": used_counts[cls], "locations": locs}
            for cls, locs in allowlisted.items()
        },
    }
    if show_dead:
        payload["dead"] = sorted(dead)
    return json.dumps(payload, indent=2, sort_keys=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Detect CSS class drift between data/ HTML, JS and CSS."
    )
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON.")
    parser.add_argument(
        "--reverse",
        action="store_true",
        help="Also report dead CSS (defined but never used).",
    )
    parser.add_argument(
        "--data-dir",
        type=Path,
        default=DATA_DIR,
        help=f"Path to data folder (default: {DATA_DIR}).",
    )
    parser.add_argument(
        "--fail-on-drift",
        action="store_true",
        help="Exit non-zero when drift is detected (for CI).",
    )
    args = parser.parse_args()

    if not args.data_dir.exists():
        print(f"ERROR: data dir not found: {args.data_dir}", file=sys.stderr)
        return 2

    used, defined, used_counts = scan_workspace(args.data_dir)
    missing, _, _ = compute_drift(used, defined, DESIGN_SYSTEM_CLASS_ALLOWLIST)

    if args.json:
        print(format_json(used, defined, used_counts, args.reverse))
    else:
        print(format_human(used, defined, used_counts, args.reverse))

    if args.fail_on_drift and missing:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
