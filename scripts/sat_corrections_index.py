#!/usr/bin/env python3
"""Build the machine-readable index of docs/sat/sergeant-d-corrections.md (TASK-1630).

The register's entries are immutable, so their metadata lives next to them in
docs/sat/sergeant-d-corrections.index.json, one record per entry. Every field is taken
from what the entry itself states; nothing is guessed:

  id                  entry number from the "## N." heading
  title               heading text
  date                last YYYY-MM-DD in the heading, else the first one on the Source line
  source              first the trailing "(...)" tag of the heading: Sergeant D -> "sergeant-d",
                      maintainer -> "maintainer", our reading / our finding / interpretation /
                      not his words / fact check -> "port-team" (our own reading, not his words).
                      Then the Source line: Robert -> maintainer; sergeantd, #alpha-testing,
                      #dev-sat-mqtt or "HH:MM UTC, message" -> sergeant-d; "no new statement" or
                      coordinator -> port-team; "same message (as entry N)" -> that entry's source.
                      "unknown" otherwise
  kind                "decision" (heading tag says decision), "interpretation" (our own reading),
                      "answer" (Source line is an answer or a reply to a queued question),
                      "correction" (heading says correct...), else "unclassified"
  discord_link        first discord.com/channels URL in the entry, else null
  supersedes          entries this one names as corrected/changed/replaced/superseded/overridden
  tasks               TASK ids named in the entry
  confirmed_by_entry  the later entry whose heading says it confirms this one, else null

    python scripts/sat_corrections_index.py            # print a summary
    python scripts/sat_corrections_index.py --update   # rewrite the index file
    python scripts/sat_corrections_index.py --check    # exit 1 when the index is stale

Run --update after adding an entry to the register.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REGISTER = ROOT / "docs" / "sat" / "sergeant-d-corrections.md"
INDEX = ROOT / "docs" / "sat" / "sergeant-d-corrections.index.json"
HEAD = re.compile(r"^## (\d+)\.\s+(.*)$", re.M)
DATE = re.compile(r"\b(20\d\d-\d\d-\d\d)\b")
LINK = re.compile(r"https://discord\.com/channels/[\d/]+")
TASK = re.compile(r"\bTASK-\d+(?:\.\d+)?\b")
SUPERSEDES = re.compile(r"(?i)\b(?:corrects|changes|replaces|supersedes|overrides)\s+entr(?:y|ies)\s+([\d,\s and-]+)")
CONFIRMS = re.compile(r"(?i)\bconfirms\b.*?\bentr(?:y|ies)\s+([\d,\s and-]+)")


def numbers(text):
    """'257-264' -> 257..264; '96, 101 and 104' -> [96, 101, 104]."""
    out = []
    for m in re.finditer(r"(\d+)\s*-\s*(\d+)|(\d+)", text):
        if m.group(1):
            out.extend(range(int(m.group(1)), int(m.group(2)) + 1))
        else:
            out.append(int(m.group(3)))
    return out


def entries(text):
    heads = list(HEAD.finditer(text))
    for i, h in enumerate(heads):
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        yield int(h.group(1)), h.group(2).strip(), text[h.end():end]


def source_line(body):
    m = re.search(r"^\*\*Source:?\*\*:?\s*(.*)$", body, re.M)
    return m.group(1).strip() if m else ""


OWN_WORDS = re.compile(r"(?i)\b(our reading|our finding|our interpretation|interpretation|not his words|fact check)\b")


def heading_tag(title):
    """The trailing '(...)' of a heading, e.g. 'Sergeant D, 2026-11-04' or 'maintainer decision, ...'."""
    m = re.search(r"\(([^()]*)\)\s*$", title)
    return m.group(1) if m else ""


def source_of(title, src, previous, by_id):
    tag = heading_tag(title).lower()
    if "sergeant d" in tag:
        return "sergeant-d"
    if "maintainer" in tag:
        return "maintainer"
    if OWN_WORDS.search(tag):
        return "port-team"
    src = src.lstrip("- ").strip()
    if re.match(r"(?i)robert\b", src):
        return "maintainer"
    if re.match(r"(?i)(sergeantd|#alpha-testing|#dev-sat-mqtt|\d\d:\d\d UTC, message)", src):
        return "sergeant-d"
    if re.match(r"(?i)(no new statement|none new|coordinator\b)", src) or OWN_WORDS.search(title):
        return "port-team"
    ref = re.match(r"(?i)(?:the )?same message as entry (\d+)|entry (\d+)\b", src)
    if ref:
        target = int(ref.group(1) or ref.group(2))
        if target in by_id:
            return by_id[target]["source"]
    if re.match(r"(?i)(the )?same message", src) and previous:
        return previous["source"]
    return "unknown"


def classify(num, title, body, previous=None, by_id=None):
    src = source_line(body)
    tag = heading_tag(title).lower()
    source = source_of(title, src, previous, by_id or {})
    if "decision" in tag:
        kind = "decision"
    elif OWN_WORDS.search(tag) or OWN_WORDS.search(title):
        kind = "interpretation"
    elif re.search(r"(?i)\breply to Q[\w.-]+|\banswer\b", src):
        kind = "answer"
    elif re.search(r"(?i)\bcorrect", title):
        kind = "correction"
    else:
        kind = "unclassified"
    dates = DATE.findall(title) or DATE.findall(src)
    link = LINK.search(body)
    sup = sorted({n for m in SUPERSEDES.finditer(body) for n in numbers(m.group(1)) if n != num})
    return {
        "id": num,
        "title": title,
        "date": dates[-1] if DATE.findall(title) else (dates[0] if dates else None),
        "source": source,
        "kind": kind,
        "discord_link": link.group(0) if link else None,
        "supersedes": sup,
        "tasks": sorted(set(TASK.findall(body + " " + title)), key=lambda t: [int(x) for x in re.findall(r"\d+", t)]),
        "confirmed_by_entry": None,
    }


def build():
    text = REGISTER.read_text(encoding="utf-8").replace("\r\n", "\n")
    records, by_id = [], {}
    for n, t, b in entries(text):
        r = classify(n, t, b, records[-1] if records else None, by_id)
        records.append(r)
        by_id[n] = r
    for n, title, _ in entries(text):
        m = CONFIRMS.search(title)
        if m:
            for target in numbers(m.group(1)):
                if target in by_id and target != n and by_id[target]["confirmed_by_entry"] is None:
                    by_id[target]["confirmed_by_entry"] = n
    return records


def render(records):
    return json.dumps({"generated_by": "scripts/sat_corrections_index.py (TASK-1630)",
                       "register": "docs/sat/sergeant-d-corrections.md",
                       "entries": records}, indent=1, ensure_ascii=False) + "\n"


def main(argv):
    records = build()
    want = render(records)
    if "--update" in argv:
        INDEX.write_text(want, encoding="utf-8", newline="\n")
        print(f"index updated: {len(records)} entries")
        return 0
    if "--check" in argv:
        have = INDEX.read_text(encoding="utf-8").replace("\r\n", "\n") if INDEX.exists() else ""
        if have != want:
            print("index is stale: run python scripts/sat_corrections_index.py --update", file=sys.stderr)
            return 1
        print("index is current")
        return 0
    from collections import Counter
    print(f"{len(records)} entries")
    for field in ("source", "kind"):
        print(field, dict(Counter(r[field] for r in records)))
    print("confirmed", sum(1 for r in records if r["confirmed_by_entry"]),
          "| with supersedes", sum(1 for r in records if r["supersedes"]),
          "| without link", sum(1 for r in records if not r["discord_link"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
