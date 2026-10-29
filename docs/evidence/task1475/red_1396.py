"""TASK-1396 AC#1 red/green: does the capture send q/D while setup() still runs?

Fake device: uptime 0 and no 'Setup finished!' line. OLD = the telnet worker
heredoc extracted from origin/dev capture-mqtt-debug.sh (as the removed test
tests/test_capture_telnet_worker.py did); NEW = scripts/otgw_capture.py.
Prints which keys reached the device after 6 s."""
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, r"D:\Users\Robert\Documents\GitHub\RvdB\wt-1475\tests")
import test_capture_tool as t  # noqa: E402

old_sh = (HERE / "old" / "capture-mqtt-debug.sh").read_text(encoding="utf-8")
worker = re.search(r"start_telnet_capture\(\) \{.*?<<'PY' &\n(.*?)\nPY\n", old_sh, re.S).group(1)

for label in ("OLD worker (origin/dev .sh)", "NEW otgw_capture.py"):
    dev = t.FakeDevice(uptime=0)
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        if label.startswith("OLD"):
            src = worker.replace('f"http://{host}/api/v2/health"', 'f"http://{host}:%d/api/v2/health"' % dev.http_port)
            (d / "w.py").write_text(src, encoding="utf-8")
            for n in ("summary.txt", "telnet.log"):
                (d / n).touch()
            p = subprocess.Popen([sys.executable, str(d / "w.py"), "127.0.0.1", str(dev.telnet_port), str(d / "telnet.log"),
                                  str(d / "summary.txt"), str(d / ".stop"), "2", "250", "250", "0", str(os.getpid())],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(6)
            (d / ".stop").touch()
        else:
            env = dict(os.environ, OTGW_CAPTURE_SETTINGS=str(d / "s.json"))
            p = subprocess.Popen([sys.executable, r"D:\Users\Robert\Documents\GitHub\RvdB\wt-1475\scripts\otgw_capture.py",
                                  "--device", "127.0.0.1", "--telnet-port", str(dev.telnet_port), "--http-port", str(dev.http_port),
                                  "--broker", "", "--output-root", str(d / "out"), "--telnet-idle-check-seconds", "0",
                                  "--skip-browser-capture", "--skip-crashlog-capture", "--skip-http-probes", "--duration", "6"],
                                 stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        p.wait(timeout=60)
    keys = "".join(dev.received)
    print(f"{label}: keys received during setup = {keys!r}; q sent: {'q' in keys}; D sent: {'D' in keys}")
    dev.shutdown()
