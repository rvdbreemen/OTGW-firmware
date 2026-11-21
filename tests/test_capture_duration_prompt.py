#!/usr/bin/env python3
"""
TASK-2965 (CLI review C4): the interactive capture asks how long to run.

Before this task the interactive flow asked only device, broker and user, so a tester who
double-clicks the launcher could not start the long run the field-run recipe asks for.
These tests call the real resolve_inputs() and save_settings() of scripts/otgw_capture.py, with
the console and the gateway replaced (no network, no device):
  a  the question lists the three choices with the agreed wording
  b  choice 2 gives the same run as --long-run --duration 86400; choice 3 is 3 days; choice 1
     leaves duration and long-run off
  c  the answer is saved and offered as the next run's default; Enter takes it
  d  a wrong answer is asked again; after 3 wrong answers the default is taken
  e  --no-prompt, --duration and --long-run skip the question
  f  without a console (stdin not a terminal) nothing is asked

Run: python tests/test_capture_duration_prompt.py
"""

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
import otgw_capture as oc  # noqa: E402


class DurationPromptTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.settings = Path(self.tmp.name) / "capture-settings.json"
        patcher = mock.patch.dict(os.environ, {"OTGW_CAPTURE_SETTINGS": str(self.settings)})
        patcher.start()
        self.addCleanup(patcher.stop)
        self.addCleanup(self.tmp.cleanup)

    def resolve(self, argv, answers, tty=True):
        """Run the real resolve_inputs(). The gateway does not answer, so the MQTT broker question
        comes first (answered with Enter = skip MQTT), then the duration question."""
        parser = oc.build_parser()
        args = parser.parse_args(oc.normalize_argv(["--device", "gw.test"] + argv))
        asked = []
        queue = list(answers)

        def fake_ask(prompt, default=""):
            asked.append((prompt, default))
            answer = queue.pop(0) if queue else ""
            return answer or default

        out = io.StringIO()
        with mock.patch.object(oc, "interactive", return_value=tty), \
                mock.patch.object(oc, "ask", side_effect=fake_ask), \
                mock.patch.object(oc, "read_gateway_mqtt", return_value=(None, "test: no gateway")), \
                contextlib.redirect_stdout(out):
            oc.resolve_inputs(args, parser)
        return args, asked, out.getvalue()

    def duration_prompts(self, asked):
        return [a for a in asked if a[0].startswith("Choice 1, 2 or 3")]

    def test_a_wording(self):
        _, asked, out = self.resolve([], ["", "1"])
        self.assertIn("How long?", out)
        self.assertIn("1) Until you press Q", out)
        self.assertIn("2) 24 hours (long run, recommended for SAT field data)", out)
        self.assertIn("3) 3 days", out)
        self.assertEqual(len(self.duration_prompts(asked)), 1)

    def test_b_choices_map_to_the_command_line_options(self):
        ref = oc.build_parser().parse_args(["--device", "gw.test", "--long-run", "--duration", "86400"])
        args, _, _ = self.resolve([], ["", "2"])
        self.assertEqual((args.long_run, args.duration), (ref.long_run, ref.duration))
        args, _, _ = self.resolve([], ["", "3"])
        self.assertEqual((args.long_run, args.duration), (True, 259200))
        args, _, _ = self.resolve([], ["", "1"])
        self.assertEqual((args.long_run, args.duration), (False, None))

    def test_c_answer_is_the_next_default(self):
        args, _, _ = self.resolve([], ["", "2"])
        oc.save_settings(args, False)
        self.assertEqual(json.loads(self.settings.read_text(encoding="utf-8"))["DurationChoice"], "2")
        args, asked, _ = self.resolve([], ["", ""])   # Enter takes the saved choice
        self.assertEqual(self.duration_prompts(asked)[0][1], "2")
        self.assertEqual((args.long_run, args.duration), (True, 86400))

    def test_d_wrong_answer_is_asked_again(self):
        args, asked, out = self.resolve([], ["", "24h", "3"])
        self.assertEqual(len(self.duration_prompts(asked)), 2)
        self.assertIn("Please type 1, 2 or 3", out)
        self.assertEqual(args.duration, 259200)
        # Input that never gives a valid answer (a stub, a broken pipe) cannot loop forever.
        args, asked, out = self.resolve([], ["", "x", "y", "z", "2"])
        self.assertEqual(len(self.duration_prompts(asked)), 3)
        self.assertIn("Using choice 1.", out)
        self.assertEqual((args.long_run, args.duration), (False, None))

    def test_e_options_skip_the_question(self):
        for argv in (["--no-prompt", "--broker", ""], ["--duration", "60"], ["--long-run"]):
            args, asked, out = self.resolve(argv, ["", "2"])
            self.assertEqual(self.duration_prompts(asked), [], argv)
            self.assertNotIn("How long?", out, argv)
        args, _, _ = self.resolve(["--duration", "60"], [""])
        self.assertEqual((args.duration, args.long_run), (60, False))

    def test_f_no_console_no_question(self):
        args, asked, out = self.resolve(["--broker", ""], [], tty=False)
        self.assertEqual(asked, [])
        self.assertEqual((args.long_run, args.duration), (False, None))


if __name__ == "__main__":
    unittest.main(verbosity=2)
