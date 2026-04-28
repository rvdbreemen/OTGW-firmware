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

This is the "Vorm A" diagnostic, intended as ground-truth input for porting
legacy widget classes (TASK-469). It will later graduate into evaluate.py as
a permanent CI gate (Vorm B / TASK-471).

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
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Set, Tuple

DATA_DIR = Path(__file__).resolve().parent.parent / "src" / "OTGW-firmware" / "data"

# A class identifier per CSS spec is essentially [A-Za-z0-9_-], starting with a letter
# or _ or -. We loosen to anything that looks like a token; numeric prefixes are
# uncommon in this project but allowed.
_CLASS_TOKEN = r"[A-Za-z_][A-Za-z0-9_-]*"

# class="foo bar baz" or class='foo bar' — captures the whole list.
_CLASS_ATTR_RE = re.compile(r"""class\s*=\s*["']([^"']+)["']""", re.IGNORECASE)

# DOM API: el.classList.add('x'), .remove('x'), .toggle('x'), .replace('x','y'), .contains('x')
_CLASSLIST_API_RE = re.compile(
    r"""classList\s*\.\s*(?:add|remove|toggle|replace|contains)\s*\(\s*["']([^"']+)["']"""
    r"""(?:\s*,\s*["']([^"']+)["'])?""",
    re.IGNORECASE,
)

# el.className = 'foo bar' (literal assignment only — dynamic strings are out of scope)
_CLASSNAME_ASSIGN_RE = re.compile(r"""\.\s*className\s*=\s*["']([^"']+)["']""")

# CSS class selector: .name (anywhere in a complex selector). Match boundaries so
# that .foo.bar reports both foo and bar; .foo:hover reports foo.
_CSS_CLASS_RE = re.compile(rf"\.({_CLASS_TOKEN})")

# Strip CSS /* … */ block comments
_CSS_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)

# Strip JS /* … */ block comments and // line comments
_JS_BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
_JS_LINE_COMMENT_RE = re.compile(r"//[^\n]*")


# ----- Pure helpers (testable; reused by future evaluate.py check_design_system_drift) -----

def strip_css_comments(text: str) -> str:
    return _CSS_COMMENT_RE.sub("", text)


def strip_js_comments(text: str) -> str:
    # Remove block comments first, then line comments. This loses newlines from block
    # comments; not relevant for class extraction.
    no_block = _JS_BLOCK_COMMENT_RE.sub("", text)
    return _JS_LINE_COMMENT_RE.sub("", no_block)


def extract_classes_from_html(content: str) -> List[Tuple[str, int]]:
    """Yield (class_name, line_number) tuples found in `class="…"` attributes."""
    hits: List[Tuple[str, int]] = []
    for i, line in enumerate(content.split("\n"), 1):
        for m in _CLASS_ATTR_RE.finditer(line):
            for cls in m.group(1).split():
                if cls:
                    hits.append((cls, i))
    return hits


def extract_classes_from_js(content: str) -> List[Tuple[str, int]]:
    """Yield (class_name, line_number) tuples for class refs in JS source."""
    cleaned = strip_js_comments(content)
    hits: List[Tuple[str, int]] = []
    for i, line in enumerate(cleaned.split("\n"), 1):
        # class="…" inside string literals (concat or template literal)
        for m in _CLASS_ATTR_RE.finditer(line):
            for cls in m.group(1).split():
                if cls and not _looks_like_template_placeholder(cls):
                    hits.append((cls, i))
        # classList.{add,remove,toggle,replace,contains}('x' [, 'y'])
        for m in _CLASSLIST_API_RE.finditer(line):
            for cls in (m.group(1), m.group(2)):
                if cls and not _looks_like_template_placeholder(cls):
                    hits.append((cls, i))
        # el.className = 'x y'
        for m in _CLASSNAME_ASSIGN_RE.finditer(line):
            for cls in m.group(1).split():
                if cls and not _looks_like_template_placeholder(cls):
                    hits.append((cls, i))
    return hits


def _looks_like_template_placeholder(token: str) -> bool:
    """Filter out '${...}' fragments and other non-class noise."""
    return token.startswith("$") or token.startswith("{") or token.endswith("}") or "{" in token


def extract_class_definitions_from_css(content: str) -> Set[str]:
    """Return set of class names defined as selectors anywhere in the CSS."""
    cleaned = strip_css_comments(content)
    return set(_CSS_CLASS_RE.findall(cleaned))


def compute_drift(
    used: Dict[str, List[str]], defined: Set[str]
) -> Tuple[Dict[str, List[str]], Set[str]]:
    """Return (used_but_not_defined, defined_but_not_used)."""
    used_set = set(used.keys())
    missing = {cls: locs for cls, locs in used.items() if cls not in defined}
    dead = defined - used_set
    return missing, dead


# ----- Driver -----

def scan_workspace(data_dir: Path) -> Tuple[Dict[str, List[str]], Set[str], Dict[str, int]]:
    """Scan all HTML/JS/CSS in data_dir. Return (used, defined, used_counts)."""
    used: Dict[str, List[str]] = defaultdict(list)
    used_counts: Dict[str, int] = defaultdict(int)
    defined: Set[str] = set()

    html_files = sorted(data_dir.glob("*.html"))
    js_files = sorted(data_dir.glob("*.js"))
    css_files = sorted(data_dir.glob("*.css"))

    for f in html_files:
        text = f.read_text(encoding="utf-8", errors="replace")
        for cls, line in extract_classes_from_html(text):
            used[cls].append(f"{f.name}:{line}")
            used_counts[cls] += 1

    for f in js_files:
        text = f.read_text(encoding="utf-8", errors="replace")
        for cls, line in extract_classes_from_js(text):
            used[cls].append(f"{f.name}:{line}")
            used_counts[cls] += 1

    for f in css_files:
        text = f.read_text(encoding="utf-8", errors="replace")
        defined |= extract_class_definitions_from_css(text)

    return used, defined, used_counts


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

    missing, dead = compute_drift(used, defined)

    out.append(f"Drift - used but NOT defined:    {len(missing)}")
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
    missing, dead = compute_drift(used, defined)
    payload = {
        "summary": {
            "defined_count": len(defined),
            "used_count": len(used),
            "drift_count": len(missing),
            "dead_count": len(dead) if show_dead else None,
        },
        "missing": {
            cls: {"count": used_counts[cls], "locations": locs}
            for cls, locs in missing.items()
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
    missing, _ = compute_drift(used, defined)

    if args.json:
        print(format_json(used, defined, used_counts, args.reverse))
    else:
        print(format_human(used, defined, used_counts, args.reverse))

    if args.fail_on_drift and missing:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
