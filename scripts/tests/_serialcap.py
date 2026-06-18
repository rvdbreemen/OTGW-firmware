import sys, time, serial
port = sys.argv[1] if len(sys.argv)>1 else "COM4"
secs = float(sys.argv[2]) if len(sys.argv)>2 else 100
out = sys.argv[3] if len(sys.argv)>3 else "serialcap.log"
try:
    s = serial.Serial(port, 115200, timeout=1)
except Exception as e:
    print("OPEN FAIL", e); sys.exit(2)
t0=time.time()
with open(out,"wb") as f:
    while time.time()-t0 < secs:
        d=s.read(4096)
        if d:
            f.write(d); f.flush()
s.close()
print("done", out)
