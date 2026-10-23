#!/usr/bin/env python3
"""
TASK-1320: orphaned capture workers.

Since TASK-1299 the telnet worker in scripts/capture-mqtt-debug.sh reconnects
forever. When the capture script is killed or its terminal is closed, the worker
used to keep running, and a later device reboot made it write into its old run
folder: "FileNotFoundError: ./20261023-105403/summary.txt" in the middle of the
next session.

The worker is a Python program inside a bash heredoc. This test extracts that
heredoc and runs it as a subprocess against a fake telnet server. It runs the
real worker code; nothing is re-implemented here.

Cases:
  a  parent PID is gone, run folder present   -> worker exits quietly, no traceback
  b  run folder deleted, server closes        -> worker exits quietly, no traceback (POSIX only:
                                                 Windows cannot delete a folder with an open log)
  c  parent alive, run folder present         -> worker keeps reconnecting, stops on the stop file

OTGW_CAPTURE_SH points the test at another copy of the script (used to show it
red on the version before the fix, whose worker takes no parent-PID argument).
Run: python tests/test_capture_orphan_worker.py
"""

import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SCRIPT = Path(os.environ.get("OTGW_CAPTURE_SH", REPO / "scripts" / "capture-mqtt-debug.sh"))
BANNER = b"OTGW fake device\r\nPress 'h' for command menu\r\n"
EXIT_BOUND = 15  # seconds an orphan may take to notice and leave


def extract_worker(text):
    m = re.search(r"start_telnet_capture\(\) \{.*?<<'PY' &\n(.*?)\nPY\n", text, re.S)
    if not m:
        raise AssertionError("telnet worker heredoc not found in " + str(SCRIPT))
    return m.group(1)


class FakeTelnet:
    def __init__(self):
        self.accepts = 0
        self.conns = []
        self._lock = threading.Lock()
        self.sock = socket.socket()
        self.sock.bind(("127.0.0.1", 0))
        self.sock.listen(5)
        self.port = self.sock.getsockname()[1]
        threading.Thread(target=self._loop, daemon=True).start()

    def _loop(self):
        while True:
            try:
                c, _ = self.sock.accept()
            except OSError:
                return
            self.accepts += 1
            with self._lock:
                self.conns.append(c)
            try:
                c.sendall(BANNER)
            except OSError:
                pass

    def close_connections(self):
        with self._lock:
            conns, self.conns = self.conns, []
        for c in conns:
            try:
                c.close()
            except OSError:
                pass

    def shutdown(self):
        try:
            self.sock.close()
        except OSError:
            pass
        self.close_connections()


def wait_for(pred, timeout):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def dead_pid():
    """PID of a process that has just exited and been reaped."""
    p = subprocess.Popen([sys.executable, "-c", "pass"])
    p.wait()
    return p.pid


class OrphanCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.worker_src = extract_worker(SCRIPT.read_text(encoding="utf-8"))

    def start(self, parent_pid):
        self.dev = FakeTelnet()
        self.addCleanup(self.dev.shutdown)
        self.tmp = tempfile.mkdtemp(prefix="orphan1320-")
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        base = Path(self.tmp)
        self.run_dir = base / "20261023-105403"
        self.run_dir.mkdir()
        worker_py = base / "worker.py"
        worker_py.write_text(self.worker_src, encoding="utf-8")
        self.summary = self.run_dir / "summary.txt"
        self.telnet_log = self.run_dir / "telnet.log"
        self.stop_file = self.run_dir / ".stop"
        self.summary.touch()
        self.telnet_log.touch()
        # argv: host port telnet_log summary stop connect_timeout reconnect_ms post_disconnect_ms idle [parent_pid]
        # HEAD's worker takes no parent PID and ignores the extra argument.
        self.proc = subprocess.Popen(
            [sys.executable, str(worker_py), "127.0.0.1", str(self.dev.port),
             str(self.telnet_log), str(self.summary), str(self.stop_file),
             "2", "250", "250", "0", str(parent_pid)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.addCleanup(self._kill)

    def _kill(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()
        for f in (self.proc.stdout, self.proc.stderr):
            try:
                f.close()
            except Exception:
                pass

    def summary_text(self):
        try:
            return self.summary.read_text(encoding="utf-8", errors="replace")
        except OSError:
            return ""

    def assert_left_quietly(self, why):
        try:
            _, err = self.proc.communicate(timeout=EXIT_BOUND)
        except subprocess.TimeoutExpired:
            self.fail("%s: worker still running after %d s" % (why, EXIT_BOUND))
        err = err.decode("utf-8", "replace")
        self.assertNotIn("Traceback", err, why + ": traceback on stderr:\n" + err)
        self.assertEqual(self.proc.returncode, 0, "%s: exit code %s, stderr:\n%s" % (why, self.proc.returncode, err))

    def test_a_dead_parent_exits_quietly(self):
        self.start(dead_pid())
        self.assert_left_quietly("dead parent")

    @unittest.skipIf(os.name == "nt", "Windows cannot delete a run folder while the worker holds telnet.log open")
    def test_b_deleted_run_dir_exits_quietly(self):
        self.start(os.getpid())
        self.assertTrue(wait_for(lambda: "Settings dump" in self.summary_text(), 10),
                        "worker never connected:\n" + self.summary_text())
        shutil.rmtree(self.run_dir)
        self.dev.close_connections()
        self.assert_left_quietly("deleted run folder")

    def test_c_live_parent_keeps_running_and_stops_on_stop_file(self):
        self.start(os.getpid())
        self.assertTrue(wait_for(lambda: "Settings dump" in self.summary_text(), 10),
                        "worker never connected:\n" + self.summary_text())
        self.dev.close_connections()
        self.assertTrue(wait_for(lambda: self.dev.accepts >= 2, 10),
                        "worker did not reconnect after the server closed:\n" + self.summary_text())
        time.sleep(1.0)
        self.assertIsNone(self.proc.poll(), "worker with a live parent and run folder stopped on its own:\n" + self.summary_text())
        self.stop_file.touch()
        try:
            _, err = self.proc.communicate(timeout=EXIT_BOUND)
        except subprocess.TimeoutExpired:
            self.fail("worker did not stop after the stop file:\n" + self.summary_text())
        self.assertNotIn("Traceback", err.decode("utf-8", "replace"))
        self.assertIn("Telnet gaps:", self.summary_text())


if __name__ == "__main__":
    unittest.main(verbosity=2)
