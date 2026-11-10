"""KD factor 0.05 against today's 0.063 for SAT's warm-up (TASK-1784, Sergeant D order 2026-11-10).

Order (Sergeant D, #dev-sat-mqtt 2026-11-10 19:33 UTC, msg 1569781945460727873, relayed by the coordinator):
"run a simulation with derivative coefficient 0.5 instead of 0.63". Our reading: the derivative gain factor
SAT_PID_KD_FACTOR (src/OTGW-firmware/SATpid.ino:28, 0.063 since entry 279 / TASK-1656) to 0.05. 0.5/0.63 and
0.05/0.063 are the same ratio (0.794), so the run is the same under either reading of the decimal point.

Same plant classes, warm-up grid, guard scenarios, scoring and pre-registered thresholds as TASK-1655
(tools/research/sat/sat_kd_warmup_sim_1655.py, docs/sat/sim-1655/thresholds.md). Only the gain formula changes,
to the firmware form with an absolute factor:
    kd = round3(f32(K) * AGGRESSION * kp)          K = 0.063 (baseline, today) or 0.05 (variant)
which is SATpid.ino:223 satPidRound3(SAT_PID_KD_FACTOR * SAT_PID_AGGRESSION_V3 * kp). The 1655 module is imported
and patched at run time (its file is not edited: its sha256 is pinned in docs/sat/sim-1655/runs.json).

Fairness: tests/test_sat_kd_005_sim.py proves (and --merge checks every baseline row) that the K = 0.063 baseline gives the same scored numbers as the
committed TASK-1655 x0.9 rows (0.9 * 0.07 and 0.063 round to the same kd, TASK-1655 notes).

Simulation only: no result is physical boiler proof.
Run: python tools/research/sat/sat_kd_005_sim.py --out <dir> --classes <list> ; then --out <dir> --merge
"""
import argparse, hashlib, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_kd_warmup_sim_1655 as k
import sat_sim_port as port

f32 = port.f32
BASE_K = 0.063          # SATpid.ino:28 today (entry 279)
VAR_K = 0.05            # Sergeant D's order, read as 0.05
# The 1655 machinery keys rows on "kd_factor"; the baseline must be 1.0 for its judge(). Map key -> absolute K.
KEYS = {1.0: BASE_K, 0.794: VAR_K}


def _gains_abs(key):
    K = f32(KEYS[key])

    def _gains(self, cv):
        self.kp = port.round3(self.coeff * f32(cv) / self.divisor)
        self.ki = port.round3(self.kp / port.AGGRESSION)
        self.kd = port.round3(K * port.AGGRESSION * self.kp)
    return _gains


def install():
    """Patch the 1655 module: absolute-K gains and the two factor keys. kd_factor.__enter__, run_class and judge
    read these module globals at call time."""
    k._gains_with = _gains_abs
    k.FACTORS = tuple(KEYS)


def recommend(per_class):
    """The TASK-1655 rule for one variant: adopt if it passes >= 6 of 8 plant classes and no guard fails anywhere;
    else keep (no 10 % gain anywhere) or trade-off (a gain but a guard fails)."""
    f = str(0.794)
    passes = sum(1 for pc in per_class.values() if pc[f]["class_pass"])
    guards_ok = all(all(pc[f]["G"].values()) for pc in per_class.values())
    if passes >= 6 and guards_ok:
        return {"choice": "0.05", "passes": passes}
    anyB = any(pc[f]["B"] for pc in per_class.values())
    return {"choice": "keep 0.063", "passes": passes,
            "reason": "trade-off: a guard fails" if anyB else "no 10 % warm-up gain in any class"}


SCORED = ("min_to_0_3_scored", "min_to_0_1_scored", "max_over", "max_over_scored", "starts_per_day",
          "share_in_0_3_scored", "rms_after_0_3", "D_approach_mean", "max_cs")


def compare_1655(rows):
    """Every 0.063 baseline row against the committed TASK-1655 x0.9 row on the same inputs (pre-registered)."""
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
    with open(os.path.join(root, "docs", "sat", "sim-1655", "runs.json"), encoding="utf-8") as f:
        ref = {(r["plant_class"],) + k._key(r): r for r in json.load(f)["runs"] if r["kd_factor"] == 0.9}
    base = [r for r in rows if r["kd_factor"] == 1.0]
    bad = [(r["plant_class"],) + k._key(r) for r in base
           if any(r[f] != ref[(r["plant_class"],) + k._key(r)][f] for f in SCORED)]
    return {"compared": len(base), "mismatches": len(bad), "first": [list(map(str, b)) for b in bad[:5]]}


def main():
    install()
    ap = argparse.ArgumentParser(); ap.add_argument("--out", required=True)
    ap.add_argument("--classes", default=",".join(k.PLANT_CLASSES))
    ap.add_argument("--merge", action="store_true")
    ap.add_argument("--thresholds-commit", default="")
    a = ap.parse_args()
    parts = os.path.join(a.out, "parts"); os.makedirs(parts, exist_ok=True)
    if not a.merge:
        for pc in a.classes.split(","):
            rows = k.run_class(pc)
            with open(os.path.join(parts, pc.replace("/", "_") + ".json"), "w", encoding="utf-8") as f:
                json.dump(rows, f, sort_keys=True)
            print(pc, json.dumps(k.judge(rows), sort_keys=True), flush=True)
        return
    per_class = {}; allrows = []
    for pc in k.PLANT_CLASSES:
        with open(os.path.join(parts, pc.replace("/", "_") + ".json"), encoding="utf-8") as f: rows = json.load(f)
        allrows.extend(rows); per_class[pc] = k.judge(rows)
    here = os.path.dirname(os.path.abspath(__file__))
    sha = lambda n: hashlib.sha256(open(os.path.join(here, n), "rb").read()).hexdigest()
    meta = {"script_sha256": sha("sat_kd_005_sim.py"), "k1655_sha256": sha("sat_kd_warmup_sim_1655.py"),
            "c19_sha256": sha("sat_closedloop_sim_1412_19.py"), "study16_sha256": sha("sat_lowload_study_1412_16.py"),
            "port_sha256": sha("sat_sim_port.py"), "thresholds_commit": a.thresholds_commit,
            "hours": k.HOURS, "dt_s": k.DT, "keys": {str(x): v for x, v in KEYS.items()}}
    rec = recommend(per_class)
    meta["baseline_vs_1655_x09"] = compare_1655(allrows)
    with open(os.path.join(a.out, "runs.json"), "w", encoding="utf-8") as f:
        json.dump({"meta": meta, "per_class": per_class, "recommendation": rec, "runs": allrows},
                  f, indent=0, sort_keys=True)
    print(json.dumps({"per_class": per_class, "recommendation": rec}, indent=1, sort_keys=True))


if __name__ == "__main__":
    main()
