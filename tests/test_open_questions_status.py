"""Status hygiene of docs/sat/open-questions.md (TASK-1629).

The question table has an "Answered" column (a checkmark plus the entry number in
sergeant-d-corrections.md) and a "Status" column (queued, asked, answered, dropped).
A row that names an answering entry must not still say asked or queued: that is the
drift TASK-1629 found (Q-1519b -> entry 163 still "asked", and 17 more).

A partial answer is written as "? <entry>" in the Answered column, not as a checkmark,
and may keep the status asked.
"""
import re
import unittest
from pathlib import Path

DOC = Path(__file__).resolve().parent.parent / "docs" / "sat" / "open-questions.md"
ROW = re.compile(r"^\| *((?:Q|Conf|QD)[\w.-]*)[^|]*\|")
STATUS_WORDS = ("queued", "asked", "answered", "dropped")


def table_rows(text):
    for line in text.splitlines():
        m = ROW.match(line)
        if not m:
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) >= 3:
            yield cells[0], cells[1], cells[2]


class OpenQuestionsStatus(unittest.TestCase):
    def setUp(self):
        self.text = DOC.read_text(encoding="utf-8")
        self.rows = list(table_rows(self.text))

    def test_table_found(self):
        self.assertGreater(len(self.rows), 50, "question table not parsed")

    def test_answered_rows_are_not_open(self):
        offenders = [
            f"{qid}: Answered '{ans}' but Status '{status[:60]}'"
            for qid, ans, status in self.rows
            if ans.startswith("✓") and not re.match(r"(answered|dropped)\b", status, re.I)
        ]
        self.assertEqual(offenders, [], "rows with an answering entry still open:\n" + "\n".join(offenders))

    def test_status_starts_with_a_status_word(self):
        # The status lives in the Status column, not in the question id cell.
        offenders = [
            f"{qid}: Status '{status[:60]}'"
            for qid, _ans, status in self.rows
            if not status.lower().startswith(STATUS_WORDS)
        ]
        self.assertEqual(offenders, [], "\n".join(offenders))

    def test_answered_entry_is_named_in_status(self):
        # "answered (entry N" / "dropped (entry N" must repeat the entry of the Answered column,
        # so the two columns cannot drift apart again.
        offenders = []
        for qid, ans, status in self.rows:
            m = re.match(r"✓\s*(\d+)", ans)
            if not m:
                continue
            if not re.search(rf"\bentr(?:y|ies)\b[^|]*\b{m.group(1)}\b", status):
                offenders.append(f"{qid}: Answered entry {m.group(1)} not named in Status '{status[:60]}'")
        self.assertEqual(offenders, [], "\n".join(offenders))

    def test_live_queue_matches_the_table(self):
        # The Live queue is derived from the table: every asked/queued row without an
        # answering entry, and nothing else. A row closed in the table but left in the list
        # (or the reverse) is the staleness this test exists for.
        open_ids = {
            qid for qid, ans, status in self.rows
            if re.match(r"(asked|queued)\b", status, re.I) and not ans.startswith("✓")
        }
        start = self.text.find("## Live queue")
        end = self.text.find("| # | Answered | Status |")
        listed = set(re.findall(r"^- \*\*([^*]+)\*\*", self.text[start:end], re.M))
        self.assertEqual(sorted(listed), sorted(open_ids),
                         "Live queue differs from the open rows of the table: "
                         "run python scripts/sat_open_questions.py --update")

    def test_history_is_below_the_live_queue(self):
        live = self.text.find("## Live queue")
        history = self.text.find("## History")
        self.assertNotEqual(live, -1, "no '## Live queue' section")
        self.assertNotEqual(history, -1, "no '## History' section")
        table = self.text.find("| # | Answered | Status |")
        self.assertLess(live, table, "the live queue belongs above the table")
        self.assertGreater(history, table, "the history belongs below the table")
        top = self.text[:table]
        self.assertNotIn("**Update 20", top, "dated update paragraphs belong in the History section")


if __name__ == "__main__":
    unittest.main()
