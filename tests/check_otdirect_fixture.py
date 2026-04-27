#!/usr/bin/env python3
"""
Static format validator for tests/otdirect_pic_parity_fixture.md.

Parses every Markdown table in the fixture and checks that every data row
contains exactly the required number of columns.  This is a format check
only — no firmware is compiled or executed.

Required columns (7):
  Command | PIC behaviour | OTDirect-expected behaviour | MsgID
  | OT data shape | Test fixture | Notes

Exit code: 0 on success, 1 if any row fails validation.

Usage:
    python tests/check_otdirect_fixture.py
"""

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
FIXTURE_PATH = REPO_ROOT / "tests" / "otdirect_pic_parity_fixture.md"

# Expected column names (case-insensitive, stripped).  The header row must
# contain all of these as sub-strings (not necessarily verbatim, to allow
# minor wording changes in the document header).
REQUIRED_COLUMN_SUBSTRINGS = [
    "command",
    "pic behaviour",
    "otdirect",
    "msgid",
    "ot data shape",
    "test fixture",
    "notes",
]

REQUIRED_COLUMN_COUNT = len(REQUIRED_COLUMN_SUBSTRINGS)


def split_table_row(line: str) -> list[str]:
    """Split a Markdown table row on '|', strip whitespace, drop empty edges."""
    parts = line.split("|")
    # First and last element are empty strings outside the outer pipes.
    return [p.strip() for p in parts[1:-1]]


def is_separator_row(cells: list[str]) -> bool:
    """Return True if every cell is a Markdown separator (---) cell."""
    return all(re.fullmatch(r":?-+:?", c) for c in cells)


def is_command_table(cells: list[str]) -> bool:
    """
    Return True if this header row belongs to a command fixture table (7 cols
    containing the required column keywords).  Other tables in the fixture
    (e.g. Ground-Truth Sources, column definitions) have fewer columns and
    are skipped.
    """
    if len(cells) != REQUIRED_COLUMN_COUNT:
        return False
    header_lower = " | ".join(c.lower() for c in cells)
    return all(kw in header_lower for kw in REQUIRED_COLUMN_SUBSTRINGS)


def validate_header(cells: list[str], table_number: int) -> list[str]:
    """
    Verify a command-table header contains each required column keyword.
    Only called when `is_command_table` is already True.
    Returns a list of error strings (empty = OK).
    """
    errors: list[str] = []
    header_lower = " | ".join(c.lower() for c in cells)
    for keyword in REQUIRED_COLUMN_SUBSTRINGS:
        if keyword not in header_lower:
            errors.append(
                f"Table {table_number} header: required column keyword "
                f'"{keyword}" not found in header: {cells}'
            )
    return errors


def validate_data_row(
    cells: list[str], row_number: int, table_number: int, required_count: int
) -> list[str]:
    """
    Verify a data row has the correct column count and that the key columns
    (Command, Test fixture) are non-empty.
    Returns a list of error strings.
    """
    errors: list[str] = []
    if len(cells) != required_count:
        errors.append(
            f"Table {table_number}, row {row_number}: expected {required_count} "
            f"columns, got {len(cells)}: {cells}"
        )
        return errors  # Cannot safely index further

    # Column 0 = Command, column 5 = Test fixture (0-indexed, within 7-col table)
    if not cells[0]:
        errors.append(
            f"Table {table_number}, row {row_number}: 'Command' column is empty."
        )
    if not cells[5]:
        errors.append(
            f"Table {table_number}, row {row_number}: 'Test fixture' column is empty."
        )
    return errors


def parse_and_validate(path: Path) -> list[str]:
    """
    Read the fixture file, find all Markdown tables, and validate each one.
    Returns a list of all error messages found.
    """
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()

    all_errors: list[str] = []
    table_number = 0
    i = 0
    in_table = False
    is_cmd_table = False   # True only for 7-column command fixture tables
    header_cells: list[str] = []
    data_row_index = 0
    cmd_tables_found = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if stripped.startswith("|"):
            cells = split_table_row(stripped)

            if not in_table:
                # Start of a new table — this line is the header
                table_number += 1
                data_row_index = 0
                header_cells = cells
                is_cmd_table = is_command_table(cells)
                if is_cmd_table:
                    cmd_tables_found += 1
                    all_errors.extend(validate_header(header_cells, table_number))
                in_table = True
                i += 1
                continue

            if is_separator_row(cells):
                # Separator row — skip, move on to data rows
                i += 1
                continue

            # Data row — only validate in command tables
            if is_cmd_table:
                data_row_index += 1
                all_errors.extend(
                    validate_data_row(
                        cells, data_row_index, table_number, len(header_cells)
                    )
                )
        else:
            in_table = False
            is_cmd_table = False
            header_cells = []
            data_row_index = 0

        i += 1

    if cmd_tables_found == 0:
        all_errors.append(
            "No command fixture tables found (7-column tables with required headers). "
            "Check that the fixture file is not empty or malformed."
        )
    else:
        # Report how many command tables were validated (informational only)
        print(f"  Found {cmd_tables_found} command fixture table(s).")

    return all_errors


def main() -> int:
    if not FIXTURE_PATH.exists():
        print(f"ERROR: fixture file not found: {FIXTURE_PATH}", file=sys.stderr)
        return 1

    print(f"Validating: {FIXTURE_PATH}")
    errors = parse_and_validate(FIXTURE_PATH)

    if errors:
        print(f"\nFAIL — {len(errors)} error(s) found:\n")
        for err in errors:
            print(f"  {err}")
        return 1

    print("OK — all table rows have the required columns.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
