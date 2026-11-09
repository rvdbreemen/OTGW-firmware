"""TASK-1738 AC2: replay the F1104 CS= sequences at 08:59:50 and 16:09:56 through the real CS senders.

Usage: python -I f1104_replay.py <transcript.txt> <SATcontrol.ino> [<SATcontrol.ino> ...]

From the capture window it takes every CmdQueue CS= insert and classifies it:
  tick    - a SATTICK line follows within 50 ms   -> satSendCS(Tboiler - 1.0f)
  resend  - 4.95-5.15 s after the previous CS= insert -> satSendCS(satCapRepeatCS(fLastSentCS))
  refresh - anything else                          -> satEnqueueIfChangedCS(Tboiler - 1.0f)
Tboiler is the last 'Tboiler = xx.xx' reply before the event (print_f88 already rounded it to 2 decimals),
converted with atof to float and minus 1.0f in C, the same float32 arithmetic as satPwmSuppressionSetpoint().
For each source it prints the replayed wire text next to the captured one, then checks:
  - every resend puts the same value on the wire as the CS= before it (no 0.1 C repeat blip)
The first two events per window are warm-up: event 1 does not fit the Tboiler - 1.0 input model and event 2
follows from its cached value. Result 2026-11-09 (TASK-1738 notes): beta.68 and the pre-fix code reproduce
the capture on 28/28 events after warm-up and show the 2 F1104 blips; the fix shows 0.
The F1104 capture (gruijter, beta.68, 2026-11-04) is user data and is NOT in the repo; pass its path.
Not a unit test (needs the capture), so it is not named test_*.py. Exit code is always 0; read the output.
"""
import re, sys, os, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import test_sat_cs_wire_matches_cache as t  # noqa: E402  (cut(), cut_tc(), STUBS, START*, END)
import test_sat_pwm_override as base          # noqa: E402

WINDOWS = (("08:59:10", "09:00:05"), ("16:09:08", "16:10:05"))   # first ~30 s = warm-up
LINE = re.compile(r"^(\d\d:\d\d:\d\d\.\d+) \([^)]*\) \s*([A-Za-z]+)\s*\(\s*\d+\): (.*)$")
INS = re.compile(r"CmdQueue: Insert queue in slot\[\d+\]:cmd\[CS=([0-9.]+)\]")
TB = re.compile(r"Read-Ack\s+> Tboiler = ([0-9.]+)")

MAIN = r"""
int main() {
  char kind[16]; long ms; char arg[32];
  while (scanf("%15s %ld %31s", kind, &ms, arg) == 3) {
    g_now = (uint32_t)ms; g_last[0] = 0;
    float v = (float)atof(arg) - 1.0f;               // Tboiler (2 dec) - fModSupOffset, float32
    if (!strcmp(kind, "tick")) satSendCS(v);
    else if (!strcmp(kind, "resend")) satSendCS(satCapRepeatCS(state.sat.fLastSentCS));
    else satEnqueueIfChangedCS(v);
    printf("%s %s\n", kind, g_last[0] ? g_last : "-");
  }
  return 0;
}
"""


def secs(hms):
    h, m, s = hms.split(":"); return int(h) * 3600 + int(m) * 60 + float(s)


def events(transcript):
    rows = []
    with open(transcript, encoding="utf-8", errors="replace") as f:
        for raw in f:
            if raw[:2] not in ("08", "09", "16"): continue
            m = LINE.match(raw.rstrip("\r\n"))
            if m: rows.append((secs(m[1]), m[2], m[3]))
    out = []
    for a, b in WINDOWS:
        lo, hi = secs(a), secs(b)
        win = [r for r in rows if lo <= r[0] <= hi]
        tb = next((x[1] for r in reversed(rows) if r[0] < lo and (x := TB.search(r[2]))), None)
        prev_t = None; seq = []
        for i, (ts, fn, txt) in enumerate(win):
            if (x := TB.search(txt)): tb = x[1]; continue
            if not (x := INS.search(txt)): continue
            tick = any(fn2 == "satControlLo" and "SATTICK" in s2 and t2 - ts <= 0.05
                       for t2, fn2, s2 in win[i + 1:i + 4])
            kind = "tick" if tick else ("resend" if prev_t is not None and 4.95 <= ts - prev_t <= 5.15 else "refresh")
            seq.append((kind, int(round(ts * 1000)), tb, "CS=" + x[1], ts))
            prev_t = ts
        out.append((a, seq))
    return out


def build(src, workdir):
    s = Path(src).read_text(encoding="utf-8")
    start = t.START_FIX if t.START_FIX in s else t.START
    cpp = Path(workdir) / "replay.cpp"
    extra = ("static CmdQueueResult addCommandToQueue(const char* c, size_t n, bool f, int p)"
             " { return addCommandToQueue(c, n, f, p, false); }\n")   # beta.68 signature
    cpp.write_text(t.STUBS + extra + t.cut(s, start, t.END) + t.cut_tc(s) + MAIN, encoding="utf-8")
    return base.compile_cpp(cpp, Path(workdir) / ("replay" + (".exe" if os.name == "nt" else "")))


def main():
    import subprocess
    wins = events(sys.argv[1])
    for src in sys.argv[2:]:
        print(f"== {Path(src).name}")
        with tempfile.TemporaryDirectory() as td:
            exe = build(src, td)
            if exe is None: sys.exit("no host C++ compiler")
            for name, seq in wins:
                inp = "".join(f"{k} {ms} {tb}\n" for k, ms, tb, _, _ in seq)
                got = subprocess.run([str(exe)], input=inp, capture_output=True, text=True, check=True).stdout.split("\n")
                got = [g.split(" ", 1)[1] for g in got if g]
                same = sum(g == c for g, (_, _, _, c, _) in list(zip(got, seq))[2:])   # events 1-2: warm-up
                blips = []
                last_wire = None
                for (k, ms, tb, cap, ts), g in zip(seq, got):
                    h = int(ts // 3600); m = int(ts % 3600 // 60); s = ts % 60
                    print(f"  {h:02d}:{m:02d}:{s:06.3f} {k:7s} Tboiler={tb:6s} capture {cap:8s} replay {g}")
                    if k == "resend" and last_wire is not None and g != last_wire: blips.append((f"{h:02d}:{m:02d}:{s:06.3f}", last_wire, g))
                    if g != "-": last_wire = g
                print(f"  window {name}: {len(seq)} events, replay==capture {same}/{len(seq)-2} (first 2 events excluded: warm-up), repeat blips {len(blips)} {blips}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
