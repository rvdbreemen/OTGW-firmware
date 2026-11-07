"""TASK-1527 round 23 (2026-11-02): candidate SAT settings for heat pumps, paired against the current settings, ten
heating seasons (TASK-1527 AC#2). Uses the round 21 library (SAT through the proven port, sat_sim_port.py).

Units: three all-electric units spanning the minimum output (Mitsubishi Ecodan WM50 1.8 kW, NIBE S2125-8 2.4 kW,
Daikin Altherma 3 R 04 3.2 kW); the four round 21 houses (underfloor); seasons 2015-16 .. 2024-25; variant "sat"
(SAT's PWM switch active) at the round 21 best curve, plus "cont" (SAT never in PWM) as reference.
Candidates (one setting changed at a time against "current" = period 1800 s, min ON/OFF 600/600 s, flow cap 40 C,
MM 100 in PWM):
  period3600, period10800, old_minon (180 s ON, no min OFF: before entries 197/198), cap35, cap45, mm0 (MM 0 in PWM).
Run: python tools/research/sat/sat_hp_candidate_sweep_study23.py --seasondir docs/sat/sim-hp-lowload/weather_season10 --outdir <dir> [--workers N]
"""
import glob, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_hp_library_study21 as lib

UNITS = ("mitsubishi_ecodan_puz_wm50", "nibe_s2125_8", "daikin_altherma3_r_erga04")
CANDS = {
    "current":     dict(PWM_PERIOD_S=1800,  PWM_MINON_S=600, PWM_MINOFF_S=600, FLOW_CAP=40.0, MM_PWM=100.0),
    "period3600":  dict(PWM_PERIOD_S=3600,  PWM_MINON_S=600, PWM_MINOFF_S=600, FLOW_CAP=40.0, MM_PWM=100.0),
    "period10800": dict(PWM_PERIOD_S=10800, PWM_MINON_S=600, PWM_MINOFF_S=600, FLOW_CAP=40.0, MM_PWM=100.0),
    "old_minon":   dict(PWM_PERIOD_S=1800,  PWM_MINON_S=180, PWM_MINOFF_S=0,   FLOW_CAP=40.0, MM_PWM=100.0),
    "cap35":       dict(PWM_PERIOD_S=1800,  PWM_MINON_S=600, PWM_MINOFF_S=600, FLOW_CAP=35.0, MM_PWM=100.0),
    "cap45":       dict(PWM_PERIOD_S=1800,  PWM_MINON_S=600, PWM_MINOFF_S=600, FLOW_CAP=45.0, MM_PWM=100.0),
    "mm0":         dict(PWM_PERIOD_S=1800,  PWM_MINON_S=600, PWM_MINOFF_S=600, FLOW_CAP=40.0, MM_PWM=0.0),
}
# Round 23c: the selectable middle period and the SAT-side hysteresis (the overshoot switch into PWM)
CANDS_C = {
    "period2400":    dict(PWM_PERIOD_S=2400),
    "ov_fw60":       dict(OV_TICKS=2),                    # firmware-faithful 60 s dwell
    "ov_margin3":    dict(OV_TICKS=2, OV_MARGIN=3.0),     # thermo-nova's margin
    "ov_margin4":    dict(OV_TICKS=2, OV_MARGIN=4.0),
    "ov_sustain300": dict(OV_TICKS=6),                    # 300 s dwell, margin 2.0
}

def job_c(a):
    cand, un, era, scr, c = a
    for k, v in CANDS["current"].items(): setattr(lib, k, v)
    lib.OV_MARGIN = 2.0; lib.OV_TICKS = 1
    for k, v in CANDS_C[cand].items(): setattr(lib, k, v)
    lib.PLANT = "ufh"; lib.SEPARATOR = False
    r = lib.run((un, "sat", c, era, scr)); r["candidate"] = cand; return r

# Round 23b: hysteresis, on SAT continuous (the heat pump's own stop/start band around CS; round 21 used 3 K / 1 K, A)
HYST = {
    "cont_hyst_3_1": dict(HP_STOP_K=3.0, HP_START_K=1.0),
    "cont_hyst_1_1": dict(HP_STOP_K=1.0, HP_START_K=1.0),
    "cont_hyst_5_3": dict(HP_STOP_K=5.0, HP_START_K=3.0),
}

def job_h(a):
    cand, un, era, scr, c = a
    for k, v in CANDS["current"].items(): setattr(lib, k, v)
    for k, v in HYST[cand].items(): setattr(lib, k, v)
    lib.PLANT = "ufh"; lib.SEPARATOR = False
    r = lib.run((un, "cont", c, era, scr)); r["candidate"] = cand; return r

def _init(w): lib._init(w)

def job(a):
    cand, un, era, scr, c = a
    for k, v in CANDS[cand].items(): setattr(lib, k, v)
    lib.PLANT = "ufh"; lib.SEPARATOR = False
    r = lib.run((un, "sat", c, era, scr)); r["candidate"] = cand; return r

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    workers = int(arg("--workers")) if "--workers" in sys.argv else 4
    best = json.load(open(os.path.join(arg("--outdir"), "round21_best_curves.json")))
    hyst = "--hyst" in sys.argv; rc = "--c" in sys.argv
    if rc: J = [(cand, un, e, s, best[f"{un}|sat"]) for cand in CANDS_C for un in UNITS for e, s in lib.HOUSES]
    elif hyst: J = [(cand, un, e, s, best[f"{un}|cont"]) for cand in HYST for un in UNITS for e, s in lib.HOUSES]
    else: J = [(cand, un, e, s, best[f"{un}|sat"]) for cand in CANDS for un in UNITS for e, s in lib.HOUSES]
    part = os.path.join(arg("--outdir"), "round23c_parts" if rc else ("round23b_parts" if hyst else "round23_parts")); os.makedirs(part, exist_ok=True); res = {}
    for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))):
        label = os.path.basename(w)[7:-5]; dest = os.path.join(part, label + ".json")
        if os.path.exists(dest): res[label] = json.load(open(dest)); continue
        with Pool(workers, initializer=_init, initargs=(w,)) as p: res[label] = p.map(job_c if rc else (job_h if hyst else job), J, chunksize=1)
        json.dump(res[label], open(dest, "w")); print(label, "done", flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round23c_switch_runs.json" if rc else ("round23b_hysteresis_runs.json" if hyst else "round23_candidate_runs.json")), "w")); print("done")
