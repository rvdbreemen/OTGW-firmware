#!/usr/bin/env python3
"""Replay the SAT heating-curve self-tune on a capture, baseline against a variant (TASK-3106, entry 502).

The REAL pure tuner block (`satCurveTune begin/end (pure)` of SATcontrol.ino plus SATcurveTune.h) is compiled on the host and fed the
control ticks of a capture, exactly as the glue does (satTuneAddTick per tick, satTuneCloseHour at every recorded hour end,
satTuneCloseDay, the coefficient change). Nothing of the tuner is re-implemented here.

Variant "under": the min_mod exclusion (relative modulation not above the boiler's minimum) applies only when the room is NOT under the
target (PID error <= 0). That is the single changed condition, text-replaced in the cut block.

Tick inputs (SatTuneTickIn) per capture tick:
  signal    = pid - curve        (sattick.csv; the firmware: clamp(pidRaw) - curve; checked against the recorded hour means)
  error     = target - room      (sattick.csv)
  chOn, flameOn, pwm, dhw        (sattick.csv: ch, flame, mode == 2 (SAT PWM mode; the `pwm` column is only the ON phase), dhw)
  relMod    = last MsgID 17 boiler READ-ACK (otframes.csv, f88), minMod = last MsgID 15 low byte (0 = never seen)
  sun, diag_event, whole-hour markers: NOT in the capture ticks; taken from the hour's RECORDED reason in sattuneh.csv
  (sun / no_sun -> every tick of that hour is excluded as that reason, diag_event -> every tick flagged, not_sat / not_controlling /
  coeff_changed / few_ticks -> the whole-hour marker). So a recorded exclusion for another reason stays in both runs.
Hours are the recorded ones: each sattuneh.csv record time ends a 3600 s hour.

Limits (also printed): a replay of recorded ticks is open loop. A coefficient change in a run changes the later curve and signal in
reality; here the later ticks stay as recorded, so only the FIRST decision and the window means are meaningful, not a trajectory.

Usage: python scripts/sat_selftune_replay.py <capture dir> [--coeff 2.3] [--src <SATcontrol.ino>] [--hdr <SATcurveTune.h>] [--json out.json]
The capture dir holds work/h/<hour>/{sattick,sattuneh,otframes}.csv or work/x/csv/*.csv.
"""
import argparse
import csv
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tests"))

DRIVER = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
using std::isfinite; using std::isnan;
#define F(x) x
#define PSTR(x) x
#define snprintf_P snprintf
@DEFS@
#include "@HDR@"
@BLOCK@
int main(int argc, char** argv) {
  float coeff = (float)atof(argv[1]);
  SatTune t; satTuneReset(t);
  char line[512];
  while (fgets(line, sizeof line, stdin)) {
    if (line[0] == 'T') {
      SatTuneTickIn in = {}; int sv, pw, pg, ch, fl, rl, dg, dh, sa;
      float se, rm, mm, sg, er;
      if (sscanf(line + 2, "%d %f %d %d %d %d %d %d %d %f %f %f %f %d", &sv, &se, &pw, &pg, &ch, &fl, &rl, &dg, &dh, &rm, &mm, &sg, &er, &sa) != 14) continue;
      in.satActive = sa != 0; in.dhw = dh != 0; in.sunValid = sv != 0; in.sunElevation = se; in.pwm = pw != 0; in.pwmGas = pg != 0;
      in.chOn = ch != 0; in.flameOn = fl != 0; in.roomLost = rl != 0; in.diagEvent = dg != 0; in.relMod = rm; in.minMod = mm;
      in.signal = sg; in.error = (er > 1e29f) ? NAN : er; if (rm > 1e29f) in.relMod = NAN;   // 1e30 = not a number (MSVC scanf cannot read nan)
      satTuneAddTick(t, in);
    } else if (line[0] == 'M') {
      satTuneMarkReason(t, (uint8_t)atoi(line + 2));
    } else if (line[0] == 'E') {
      char tag[64]; strncpy(tag, line + 2, sizeof tag - 1); tag[sizeof tag - 1] = 0; tag[strcspn(tag, "\r\n")] = 0;
      SatTuneHourOut h; satTuneCloseHour(t, h, 90, 120);
      printf("H|%s|%u|%d|%s|%d|%.3f|%u", tag, (unsigned)h.hour, (int)h.counted, satTuneReasonName(h.reason), (int)h.haveMean, (double)h.mean, (unsigned)h.ticks);
      if (h.dayComplete) {
        SatTuneDayOut d; satTuneCloseDay(t, coeff, 30000, d);
        printf("|D|%d|%u|%s|%.3f|%.3f|%.3f|%.2f|%u", d.decision, (unsigned)d.daysInWindow, d.reason, (double)d.mean3,
               (double)d.phMean3[SATP_WARMUP], (double)d.phMean3[SATP_FLOATING], (double)d.newCoeff, (unsigned)d.counted);
        if (d.decision != 0) { coeff = d.newCoeff; satTuneNoteCoeffChange(t, false); }
      }
      printf("\n");
    }
  }
  return 0;
}
"""

REASON_ID = {"none": 0, "not_sat": 1, "dhw": 2, "no_sun": 3, "sun": 4, "pwm": 5, "flame_off": 6, "min_mod": 7, "coeff_changed": 8,
             "no_ticks": 9, "not_controlling": 10, "few_ticks": 11, "no_room": 12, "diag_event": 13}
MARKERS = {"not_sat", "not_controlling", "coeff_changed", "few_ticks", "no_ticks"}


def tsec(s):
    h, m, r = s.split(":")
    return int(h) * 3600 + int(m) * 60 + float(r)


class Clock:
    """Device time of day -> seconds on a monotonic axis across midnights."""
    def __init__(self):
        self.day = 0
        self.last = None

    def __call__(self, s):
        v = tsec(s)
        if self.last is not None and v < self.last - 43200:
            self.day += 1
        self.last = v
        return self.day * 86400 + v


def read_csv(p):
    with open(p, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def gather(cap):
    cap = Path(cap)
    dirs = sorted((cap / "work" / "h").glob("*/")) if (cap / "work" / "h").is_dir() else []
    if not dirs:
        dirs = [cap / "work" / "x" / "csv"]
    ticks, hours, frames = [], [], []
    ck_t, ck_h, ck_f = Clock(), Clock(), Clock()
    for d in dirs:
        for r in (read_csv(d / "sattick.csv") if (d / "sattick.csv").is_file() else []):
            ticks.append((ck_t(r["time"]), r))
        for r in (read_csv(d / "sattuneh.csv") if (d / "sattuneh.csv").is_file() else []):
            hours.append((ck_h(r["time"]), r))
        if (d / "otframes.csv").is_file():
            for r in read_csv(d / "otframes.csv"):
                if r["source"] == "B" and r["msgid"] in ("15", "17") and r["type"] == "READ-ACK":
                    frames.append((ck_f(r["device_time"]), r["msgid"], r))
    ticks.sort(key=lambda x: x[0]); hours.sort(key=lambda x: x[0]); frames.sort(key=lambda x: x[0])
    return ticks, hours, frames


def derive_min_mod(ticks, frames):
    """Lowest relative modulation (> 0) the boiler reported while the flame was on (sattick flame=1), whole capture."""
    fi, rel, lo = 0, None, None
    for tt, r in ticks:
        while fi < len(frames) and frames[fi][0] <= tt:
            if frames[fi][1] == "17":
                rel = float(frames[fi][2]["f88"])
            fi += 1
        if r.get("flame") == "1" and rel is not None and rel > 0 and (lo is None or rel < lo):
            lo = rel
    return lo


def build_stream(ticks, hours, frames, derive_minmod=False):
    out = []
    fi = 0
    relmod, minmod = float("nan"), 0.0
    derived = derive_min_mod(ticks, frames) if derive_minmod else None
    ti = 0
    n_t = len(ticks)
    for te, hr in hours:
        ts = te - 3600.0
        reason = hr["reason"]
        marks = []
        if reason in MARKERS:
            marks.append(REASON_ID[reason])
        while ti < n_t and ticks[ti][0] < ts:
            ti += 1
        j = ti
        while j < n_t and ticks[j][0] < te:
            tt, r = ticks[j]
            while fi < len(frames) and frames[fi][0] <= tt:
                _, mid, fr = frames[fi]
                if mid == "17":
                    relmod = float(fr["f88"])
                else:
                    minmod = float(int(fr["value_lb"]))
                fi += 1
            def num(k):
                try:
                    return float(r[k])
                except (KeyError, ValueError):
                    return float("nan")
            if derive_minmod and minmod == 0.0 and derived is not None:
                mm_used = derived
            else:
                mm_used = minmod
            room, target = num("room"), num("target")
            roomlost = 0 if (r.get("rv", "1") == "1" and room == room) else 1
            sig = num("pid") - num("curve")
            if sig != sig:
                sig = 0.0   # the firmware sends signal 0 (curve only) without a PID value; such a tick is no_room or not counted anyway
            err = (target - room) if not roomlost else float("nan")
            sun_valid = 0 if reason == "no_sun" else 1
            sun_el = 90.0 if reason == "sun" else 0.0
            diag = 1 if reason == "diag_event" else 0
            out.append("T %d %.1f %s %d %s %s %d %d %d %.2f %.0f %.4f %.4f 1" % (
                sun_valid, sun_el, "1" if r.get("mode") == "2" else "0", 1, r["ch"], r["flame"], roomlost, diag, int(r["dhw"] or 0),
                relmod if relmod == relmod else 1e30, mm_used, sig, err if err == err else 1e30))
            j += 1
        ti = j
        for m in marks:
            out.append("M %d" % m)
        out.append("E %.0f" % te)
    return "\n".join(out) + "\n"


def compile_driver(src_ino, hdr, variant, workdir, step=None, adaptive=False):
    text = Path(src_ino).read_text(encoding="utf-8").replace("\r\n", "\n")
    a = text.index("// --- satCurveTune begin")
    b = text.index("// --- satCurveTune end (pure)")
    block = text[a:b]
    if variant.startswith("under"):
        thr = "0.0f" if variant == "under" else "%sf" % variant.split("_")[1]   # "under" = error > 0, "under_0.1" = error > 0.1 (clearly under target)
        old = "if (!pwmGas && !(in.relMod > minMod))         return SATT_MIN_MOD;"
        assert old in block, "min_mod line not found: the tuner changed"
        block = block.replace(old, "if (!pwmGas && !(in.relMod > minMod) && !(in.error > %s)) return SATT_MIN_MOD;   // VARIANT: only when the room is not under target" % thr + "")
    if adaptive:   # a step of 2 x SAT_CT_STEP when the window mean is more than 3 C outside the band
        o1 = "target = coeff + SAT_CT_STEP;"; o2 = "target = coeff - SAT_CT_STEP;"
        assert o1 in block and o2 in block, "step lines not found: the tuner changed"
        block = block.replace(o1, "target = coeff + SAT_CT_STEP * (fabsf(out.mean3) > 3.0f ? 2.0f : 1.0f);").replace(o2, "target = coeff - SAT_CT_STEP * (fabsf(out.mean3) > 3.0f ? 2.0f : 1.0f);")
    defs = ("#define SAT_CT_STEP %sf" % step) if step else ""
    tag = "%s_%s_%s" % (variant.replace(".", "_"), step or "d", "ad" if adaptive else "fx")
    cpp = Path(workdir) / ("tune_%s.cpp" % tag)
    cpp.write_text(DRIVER.replace("@DEFS@", defs).replace("@HDR@", str(Path(hdr).resolve()).replace("\\", "/")).replace("@BLOCK@", block), encoding="utf-8")
    from test_sat_pwm_override import compile_cpp
    exe = compile_cpp(cpp, Path(workdir) / ("tune_%s.exe" % tag))
    if exe is None:
        sys.exit("no host C++ compiler")
    return exe


def run(exe, stream, coeff):
    p = subprocess.run([str(exe), str(coeff)], input=stream, capture_output=True, text=True, check=True)
    rows = []
    for ln in p.stdout.splitlines():
        f = ln.split("|")
        row = {"end": f[1], "hour": int(f[2]), "counted": int(f[3]), "reason": f[4], "mean": float(f[6]) if f[5] == "1" else None, "ticks": int(f[7])}
        if len(f) > 8 and f[8] == "D":
            row.update(decision=int(f[9]), window=int(f[10]), dreason=f[11], mean3=float(f[12]), wmean=float(f[13]), fmean=float(f[14]),
                       newcoeff=float(f[15]))
        rows.append(row)
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("capture")
    ap.add_argument("--coeff", type=float, default=2.0)
    ap.add_argument("--src", default=str(REPO / "src/OTGW-firmware/SATcontrol.ino"))
    ap.add_argument("--hdr", default=str(REPO / "src/OTGW-firmware/SATcurveTune.h"))
    ap.add_argument("--json")
    a = ap.parse_args()
    ticks, hours, frames = gather(a.capture)
    print("ticks %d, recorded hours %d, MsgID 15/17 acks %d" % (len(ticks), len(hours), len(frames)))
    stream = build_stream(ticks, hours, frames)
    res = {}
    with tempfile.TemporaryDirectory() as td:
        for v in ("base", "under", "under_0.1"):
            res[v] = run(compile_driver(a.src, a.hdr, v, td), stream, a.coeff)
    rec = {h[1]["hour"]: h[1] for h in hours}
    print("\nhour | recorded: counted reason mean | replay base: counted reason mean | variant: counted reason mean decision")
    for b, u in zip(res["base"], res["under"]):
        r = next((x for x in hours if abs(x[0] - float(b["end"])) < 1), None)
        rr = r[1] if r else {}
        fm = lambda m: ("%+.2f" % m) if m is not None else "  na "
        print("%3d | %s %-12s %s | %s %-12s %s | %s %-12s %s %s" % (
            b["hour"], rr.get("counted", "?"), rr.get("reason", "?"), rr.get("mean", "?"),
            b["counted"], b["reason"], fm(b["mean"]), u["counted"], u["reason"], fm(u["mean"]),
            ("dec=%+d win=%d %s" % (u["decision"], u["window"], u["dreason"])) if "decision" in u else ""))
    for v in ("base", "under", "under_0.1"):
        decs = [r for r in res[v] if r.get("decision")]
        cnt = sum(r["counted"] for r in res[v])
        print("\n%s: counted hours %d of %d, decisions %s" % (v, cnt, len(res[v]), [(r["hour"], r["decision"], r["newcoeff"]) for r in decs]))
    if a.json:
        Path(a.json).write_text(json.dumps(res, indent=1), encoding="utf-8")


if __name__ == "__main__":
    main()
