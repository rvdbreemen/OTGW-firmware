"""TASK-1527 round 17: SAT's own PWM with the heat-pump period extended beyond today's limit (Robert 2026-10-31:
"Vergelijk de aanpak met SAT en laat PWM oplopen verder dan nu mogelijk is").

Today SAT offers heat pumps 1800 s or 2400 s (SATcontrol.ino:440-446, entries 40/50). This round sweeps the SAT PWM
period over 30 / 40 / 60 / 90 / 120 / 180 / 240 minutes with SAT's mechanism as it is (ON time fixed at the start of
each period, thermo-nova duty formula, minimum ON 180 s; round 14 showed the real mapper works at 7200 s), on every
heat-pump type we model, over full heating seasons (1 Oct .. 30 Apr: 2010-11, 2013-14, 2025-26), 2 seeds:
- radiators (round 4/5b plant): modulating 30 %, modulating 60 %, on/off;
- underfloor heating (round 12-15 plant, K fixed on the reference winter): on/off, heavy and light screed; here also
  the time-proportional variant (ON time re-evaluated inside the period) for the same periods.
Comparison partners come from rounds 15 and 16 (degree minutes, current control).
Run: python scripts/sat_hp_period_sweep_study17.py --seasondir <dir> --krs <round5c_runs.json> --outdir <dir> [--workers N]
"""
import glob, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b
import sat_hp_onoff_study12 as h12
import sat_hp_onoff_study13 as h13
import sat_hp_season_study15 as s15

PERIODS_MIN = (30, 40, 60, 90, 120, 180, 240)

def _init(w, krs):
    s15._init(w, krs); s4.HP_CTRL = "sat"

def job(a):
    if a[0] == "rad":
        _, per, era, mass, src, sd = a
        s4.HP_CTRL = "sat"; s4.HP_PERIOD_S = per * 60; kr = s5b.KRS[f"{era}|{mass}|1.8"]
        r = s4.run((("none",), era, mass, kr, round(1.8 - s5b._cb(era, mass, kr), 3), 30, sd, False, src))
        r.update(plant="rad", period_min=per); return r
    _, v, per, era, scr, sd = a
    r = h13.run((v, per, era, "heavy", scr, sd)); r.update(plant="ufh", period_min=per); return r

def jobs():
    out = [("rad", p, e, m, src, sd) for p in PERIODS_MIN for e, m in s5b.HOUSES for src in ("hp30", "hp60", "hp100") for sd in (1, 2)]
    out += [("ufh", v, p, e, scr, sd) for v in ("pwm", "tp") for p in PERIODS_MIN for e in ("4_1992-2012", "5_2015", "6_BENG2021") for scr in (8.0e6, 3.0e6) for sd in (1, 2)]
    return out

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    krs = json.load(open(arg("--krs")))["krs"]; res = {}
    workers = int(arg("--workers")) if "--workers" in sys.argv else 8
    part = os.path.join(arg("--outdir"), "round17_parts"); os.makedirs(part, exist_ok=True)
    for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))):
        label = os.path.basename(w)[7:-5]; dest = os.path.join(part, label + ".json")
        if os.path.exists(dest): res[label] = json.load(open(dest)); print(label, "exists, skipped", flush=True); continue
        with Pool(workers, initializer=_init, initargs=(w, krs)) as p: res[label] = p.map(job, jobs(), chunksize=2)
        tmp = dest + ".tmp"; json.dump(res[label], open(tmp, "w")); os.replace(tmp, dest)
        print(label, "done", len(res[label]), flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round17_hp_period_sweep_runs.json"), "w"))
