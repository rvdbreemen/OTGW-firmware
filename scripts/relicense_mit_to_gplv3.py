"""
relicense_mit_to_gplv3.py - structural MIT -> GPLv3 relicensing for OTGW-firmware

SCOPE (deliberately narrow, see the project's own review of this before running):
  This project's source tree contains code with THREE DIFFERENT copyright holders,
  not just Robert van den Breemen. Only files where Robert van den Breemen is the
  SOLE copyright holder are relicensed. Everything else is explicitly excluded and
  left untouched, on purpose:
    - src/libraries/OpenTherm/       -- (c) Ihor Melnyk (third party, MIT)
    - src/libraries/OTGWSerial/      -- (c) Schelte Bron (third party, MIT)
    - src/OTGW-firmware/FSexplorer.ino -- (c) Jens Fleischer + Robert (LGPL 2.1,
                                            shared authorship, different license)
    - other-projects/                -- external upstream reference material,
                                          read-only by project convention (CLAUDE.md)

WHAT THIS SCRIPT TOUCHES:
  - src/OTGW-firmware/**/*.{ino,h,cpp}   (except FSexplorer.ino, excluded above)
  - src/libraries/SimpleTelnet/**/*.h    (Robert's own vendored library)
  - src/libraries/Platform/**/*.h        (Robert's own)
  - src/libraries/SimpleTelnet/LICENSE   (Robert's own, full GPLv3 text)

SAFETY CHECK (defense in depth, beyond the static exclusion list above): before
editing any file, this script scans it for "Copyright (c)" lines. If ANY line
names a holder other than "Robert van den Breemen", the file is SKIPPED and
reported, never edited. This catches any file not already covered by the
exclusion list above.

WHAT IT DOES NOT DO:
  - Does not touch the root LICENSE file (handle that separately -- it needs the
    full, verbatim, unmodified official GPLv3 text from
    https://www.gnu.org/licenses/gpl-3.0.txt, not a script-generated copy, since
    altering the GPL's own license text is against the GPL's own terms).
  - Does not touch README.md or CHANGELOG.md (separate, deliberate edits).

USAGE
  python scripts/relicense_mit_to_gplv3.py --dry-run   # preview, no writes
  python scripts/relicense_mit_to_gplv3.py              # apply
"""

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

INCLUDE_GLOBS = [
    ("src/OTGW-firmware", ["*.ino", "*.h", "*.cpp"]),
    ("src/libraries/SimpleTelnet", ["*.h", "*.cpp"]),
    ("src/libraries/Platform", ["*.h", "*.cpp"]),
]

EXCLUDE_FILENAMES = {"FSexplorer.ino"}

OWNER = "Robert van den Breemen"

# The long-form MIT permission/disclaimer block, in either of the two endings
# observed in this codebase ("...IN THE SOFTWARE." or "...for details.").
# Matched as ONE block from "Permission is hereby granted" through the end of
# the disclaimer paragraph, comment-syntax-agnostic (works whether lines start
# with "*", "**", or nothing -- re.MULTILINE + a permissive leading-token class).
MIT_BLOCK_RE = re.compile(
    r"[ \t*]*Permission is hereby granted.*?"
    r"(?:IN THE SOFTWARE\.|for details\.)\s*",
    re.DOTALL,
)

GPLV3_NOTICE_LINES = [
    "This program is free software: you can redistribute it and/or modify",
    "it under the terms of the GNU General Public License as published by",
    "the Free Software Foundation, either version 3 of the License, or",
    "(at your option) any later version.",
    "",
    "This program is distributed in the hope that it will be useful,",
    "but WITHOUT ANY WARRANTY; without even the implied warranty of",
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the",
    "GNU General Public License for more details.",
    "",
    "You should have received a copy of the GNU General Public License",
    "along with this program. If not, see <https://www.gnu.org/licenses/>.",
]

# (pattern, replacement) pairs for the short pointer-line / one-liner variants.
POINTER_REPLACEMENTS = [
    (re.compile(r"TERMS OF USE:\s*MIT License\.\s*(See .*)"), r"TERMS OF USE: GNU GPLv3. \1"),
    (re.compile(r"Copyright \(c\) (\d{4}(?:-\d{4})?) Robert van den Breemen\s*(?:—|--)\s*MIT License"),
     r"Copyright (c) \1 Robert van den Breemen -- GNU GPLv3"),
    (re.compile(r"MIT License\s*(?:—|--)\s*see LICENSE"), "GNU GPLv3 -- see LICENSE"),
]


def find_candidate_files():
    files = []
    for rel_dir, patterns in INCLUDE_GLOBS:
        base = REPO_ROOT / rel_dir
        if not base.exists():
            continue
        for pattern in patterns:
            for path in base.rglob(pattern):
                if path.name in EXCLUDE_FILENAMES:
                    continue
                files.append(path)
    return sorted(set(files))


def other_copyright_holder(text):
    """Return the offending line if any Copyright (c) line names someone other
    than OWNER; None if the file is clean (only OWNER, or no copyright line)."""
    for line in text.splitlines():
        if "Copyright (c)" in line or "Copyright(c)" in line:
            if OWNER not in line:
                return line.strip()
    return None


def gplv3_notice_with_prefix(matched_block):
    """Detect the comment-line prefix ('* ', '** ', or none) from the matched
    MIT block's first content line, and reapply it to every GPLv3 notice line --
    otherwise the replacement falls out of the file's comment-block style even
    though it stays syntactically valid (C/C++ doesn't require a '*' on comment
    continuation lines, but every other block in this codebase has one)."""
    first_line = matched_block.strip().splitlines()[0]
    m = re.match(r"^([ \t]*\**[ \t]*)", first_line)
    prefix = m.group(1) if m else ""
    if not prefix.strip("* \t"):
        pass  # prefix is purely whitespace/asterisks, as expected
    lines = []
    for line in GPLV3_NOTICE_LINES:
        lines.append((prefix + line) if line else prefix.rstrip())
    return "\n".join(lines) + "\n"


def convert_text(text):
    """Returns (new_text, changed: bool)."""
    changed = False
    new_text = text

    # Some files (e.g. jsonChunked.h) repeat the full MIT block once per
    # section -- replace ALL occurrences, each with its own prefix detection,
    # not just the first (count=1 would silently leave later copies as MIT).
    if MIT_BLOCK_RE.search(new_text):
        new_text = MIT_BLOCK_RE.sub(lambda m: gplv3_notice_with_prefix(m.group(0)), new_text)
        changed = True

    for pattern, repl in POINTER_REPLACEMENTS:
        new_text2, n = pattern.subn(repl, new_text)
        if n:
            new_text = new_text2
            changed = True

    # Catch-all: any remaining bare "MIT License" mention this file's specific
    # patterns didn't cover. Flag rather than guess at a replacement.
    if "MIT License" in new_text:
        return new_text, changed, True  # changed, but needs manual follow-up

    return new_text, changed, False


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--dry-run", action="store_true", help="Preview only, write nothing")
    args = p.parse_args()

    candidates = find_candidate_files()
    print(f"Scanning {len(candidates)} candidate files...\n")

    converted, skipped, needs_review = [], [], []

    for path in candidates:
        text = path.read_text(encoding="utf-8")
        offender = other_copyright_holder(text)
        if offender:
            skipped.append((path, offender))
            continue

        if "MIT License" not in text and "Permission is hereby granted" not in text:
            continue  # nothing to convert in this file

        new_text, changed, leftover = convert_text(text)
        if not changed:
            continue

        if leftover:
            needs_review.append(path)

        if not args.dry_run:
            path.write_text(new_text, encoding="utf-8")
        converted.append(path)

    rel = lambda pth: pth.relative_to(REPO_ROOT)
    print(f"Converted ({'would be' if args.dry_run else ''} {len(converted)} files):")
    for path in converted:
        flag = "  <-- has a leftover 'MIT License' mention, review manually" if path in needs_review else ""
        print(f"  {rel(path)}{flag}")

    print(f"\nSkipped ({len(skipped)} files, third-party copyright detected):")
    for path, offender in skipped:
        print(f"  {rel(path)}: {offender}")

    if needs_review:
        print(f"\n{len(needs_review)} file(s) need manual review (see <-- above).")

    print(f"\n{'DRY RUN -- no files written.' if args.dry_run else 'Done.'}")
    return 1 if needs_review else 0


if __name__ == "__main__":
    sys.exit(main())
