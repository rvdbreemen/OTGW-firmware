"""TASK-1242: settings.ini must be valid JSON.

writeSettings() (settingStuff.ino) writes one key per line and passes withComma
per call. Only the last writeJson*KV call may pass false; a false in the middle
leaves out a comma and the downloaded settings.ini no longer parses as JSON.
The firmware reader is line-based and does not notice, so this test guards it.

Run: python tests/test_settings_writer_json.py
"""
import re
import unittest
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src" / "OTGW-firmware" / "settingStuff.ino"
CALL = re.compile(r"writeJson(?:String|Bool|Int|Float)KV\s*\(\s*file\s*,\s*F\(\"(\w+)\"\).*?,\s*(true|false)\s*\)\s*;")


def writer_calls():
    text = SRC.read_text(encoding="utf-8", errors="replace")
    start = text.index("void writeSettings(bool show)")
    end = text.index("} // writeSettings()", start)
    return CALL.findall(text[start:end])


class SettingsWriterCommas(unittest.TestCase):
    def test_only_last_entry_has_no_comma(self):
        calls = writer_calls()
        self.assertGreater(len(calls), 100, "writer calls not found; regex out of date?")
        without_comma = [key for key, comma in calls if comma == "false"]
        self.assertEqual(without_comma, [calls[-1][0]],
                         "withComma=false on a non-last entry breaks settings.ini JSON")


if __name__ == "__main__":
    unittest.main()
