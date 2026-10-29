"""Bench helper (TASK-1475 evidence): show the debug-toggle states in the telnet
banner, and optionally put them back to a given state map.

  python toggles.py 192.168.88.61                       # print states
  python toggles.py 192.168.88.61 "OTmsg=0,MQTT=1"      # restore, then print again

Only keys of toggles listed in the banner are sent (never Z/Y, never simulators).
"""
import socket
import sys
import time

sys.path.insert(0, r"D:\Users\Robert\Documents\GitHub\RvdB\wt-1475\scripts")
from otgw_capture import parse_toggles, BANNER_FOOTER, SIMULATOR_LABELS  # noqa: E402


def banner(host):
    s = socket.create_connection((host, 23), timeout=6)
    s.settimeout(0.5)
    text, end = "", time.monotonic() + 6
    while time.monotonic() < end and not BANNER_FOOTER.search(text):
        try:
            chunk = s.recv(4096)
            if not chunk:
                break
            text += chunk.decode("ascii", "replace")
        except socket.timeout:
            pass
    return s, parse_toggles(text)


host = sys.argv[1]
s, toggles = banner(host)
print("before:", ", ".join(f"{k}={v[1]}" for k, v in toggles.items()))
if len(sys.argv) > 2:
    want = dict(item.split("=", 1) for item in sys.argv[2].split(","))
    for label, (key, state) in toggles.items():
        if label in SIMULATOR_LABELS or label not in want or want[label] == state:
            continue
        assert key not in ("Z", "Y")
        s.sendall(key.encode("ascii"))
        print(f"sent '{key}' for {label}")
        time.sleep(0.4)
    s.close()
    time.sleep(1.5)
    s, toggles = banner(host)
    print("after: ", ", ".join(f"{k}={v[1]}" for k, v in toggles.items()))
s.close()
