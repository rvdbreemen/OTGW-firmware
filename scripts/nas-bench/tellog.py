"""Telnet logger for an OTGW unit (Python 3.8+, stdlib only). Raw bytes to <out>/telnet.raw, events to <out>/tel.log.
Exactly ONE telnet client per unit: do not run a second logger (laptop or NAS) on the same unit.
Reconnects at once on a closed socket and after <silence_s> without data (default 30; a reboot leaves the old socket open without FIN, so keep this short).
Optional: bytes written to <out>/send.txt are sent to the unit, then the file is removed.
Usage: tellog.py <ip> <outdir> <minutes> [silence_s]"""
import os, socket, sys, time
IP, OUT, END = sys.argv[1], sys.argv[2], time.time() + float(sys.argv[3]) * 60
SILENCE = float(sys.argv[4]) if len(sys.argv) > 4 else 30.0
os.makedirs(OUT, exist_ok=True)
def w(m):
    with open(os.path.join(OUT, "tel.log"), "a") as f: f.write(time.strftime("%Y-%m-%d %H:%M:%S ") + m + "\n")
raw = open(os.path.join(OUT, "telnet.raw"), "ab")
w("start ip=%s minutes=%s" % (IP, sys.argv[3]))
while time.time() < END:
    try:
        s = socket.create_connection((IP, 23), 5); s.settimeout(1); w("telnet connected"); last = time.time()
        while time.time() < END:
            sp = os.path.join(OUT, "send.txt")
            if os.path.exists(sp):
                data = open(sp, "rb").read(); os.remove(sp); s.sendall(data); w("sent %r" % data)
            try:
                d = s.recv(65536)
                if not d: w("telnet closed"); break
                raw.write(d); raw.flush(); last = time.time()
            except socket.timeout:
                if time.time() - last > SILENCE: w("telnet %d s silent, reconnect" % SILENCE); break
        s.close()
    except OSError as e:
        w("telnet connect failed %s" % type(e).__name__)
    time.sleep(1)
w("end")
