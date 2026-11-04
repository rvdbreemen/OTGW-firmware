#!/usr/bin/env python3
"""TASK-1610: scripts/discord-post-build.sh builds an ASCII-only JSON payload.

The payload reaches the Windows-native curl.exe as a command-line argument, and Windows converts
argv through the ANSI code page, so a non-ASCII character in the -m text ("Andre" with an accent)
arrived on Discord as a replacement character. The fix builds the JSON with \\uXXXX escapes.

The test cuts the real payload-building lines out of the script (from 'content=' to the line that
sets 'payload=' or the escape line before it, depending on the version) and runs them in bash with
a message holding an accent, a quote, a backslash and a tab. It then checks that the payload is
pure ASCII and that json.loads() gives back the text without the tab.

OTGW_POST_SCRIPT=<path> runs it against another copy of the script (old-vs-fix).
Needs bash on PATH (Git Bash on Windows). Run: python tests/test_discord_post_payload.py
"""
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SCRIPT = Path(os.environ.get("OTGW_POST_SCRIPT", REPO / "scripts" / "discord-post-build.sh"))


def payload_block(text):
    lines = text.splitlines()
    start = next(i for i, l in enumerate(lines) if l.startswith("content="))
    end = next(i for i, l in enumerate(lines) if l.startswith("payload="))
    return "\n".join(l for l in lines[start:end + 1] if not l.lstrip().startswith("#"))


@unittest.skipUnless(shutil.which("bash"), "bash not on PATH")
class PostPayload(unittest.TestCase):
    def build(self, extra):
        block = payload_block(SCRIPT.read_text(encoding="utf-8"))
        prog = ("refuse() { echo REFUSE \"$*\"; exit 3; }\nversion=2.0.0-beta.79\nhash=a9dd048\n"
                "extra=\"$EXTRA\"\n" + block + "\nprintf '%s' \"$payload\"\n")
        env = dict(os.environ, EXTRA=extra)
        # The program goes in a file, not through "bash -c": on Windows the -c argument itself passes the
        # command-line conversion that mangles backslashes and quotes. The full path from PATH, because a
        # bare "bash" makes Windows CreateProcess find System32\bash.exe (WSL) first.
        with tempfile.NamedTemporaryFile("w", suffix=".sh", delete=False, encoding="utf-8", newline="\n") as f:
            f.write(prog)
        try:
            r = subprocess.run([shutil.which("bash"), f.name], capture_output=True, env=env)
        finally:
            os.unlink(f.name)
        self.assertEqual(r.returncode, 0, r.stderr.decode("utf-8", "replace"))
        return r.stdout

    def test_payload_is_ascii_and_round_trips(self):
        msg = 'thanks André Brait: "quoted" back\\slash\tend'
        out = self.build(msg)
        self.assertTrue(all(b < 128 for b in out), "payload holds non-ASCII bytes: %r" % out)
        doc = json.loads(out.decode("ascii"))
        self.assertTrue(doc["content"].endswith(msg.replace("\t", "")), doc["content"])
        self.assertEqual(doc["allowed_mentions"], {"parse": []})

    def test_plain_ascii_message_unchanged(self):
        doc = json.loads(self.build("TASK-1602: MM/CH every 60 s").decode("ascii"))
        self.assertIn("TASK-1602: MM/CH every 60 s", doc["content"])
        self.assertIn("Build ready: `2.0.0-beta.79+a9dd048`", doc["content"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
