"""TASK-1784: charts and a numbers table for the KD 0.05 vs 0.063 study, from <dir>/runs.json.
Context column: the committed TASK-1655 x1.0 (0.07) median t03 per class (docs/sat/sim-1655/runs.json), not re-run.
Run: python tools/research/sat/sat_kd_005_chart.py --dir docs/sat/sim-1784
"""
import argparse, json, os, statistics
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
VAR = "0.794"
LABEL = {"3_1983-92": "1983-92", "5_2015": "2015"}


def short(pc):
    era, mass, em = pc.split("/")
    return f"{LABEL[era]} {mass} {em}"


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--dir", required=True); a = ap.parse_args()
    with open(os.path.join(a.dir, "runs.json"), encoding="utf-8") as f: d = json.load(f)
    with open(os.path.join(ROOT, "docs", "sat", "sim-1655", "runs.json"), encoding="utf-8") as f: old = json.load(f)
    pcs = list(d["per_class"])
    med = statistics.median
    t = lambda runs, pc, fac: med([r["min_to_0_3_scored"] for r in runs if r["plant_class"] == pc
                                   and r["kind"] == "warmup" and r["kd_factor"] == fac])
    rows = []
    for pc in pcs:
        s = d["per_class"][pc][VAR]
        rows.append({"class": short(pc), "t03_007": t(old["runs"], pc, 1.0), "t03_0063": t(d["runs"], pc, 1.0),
                     "t03_005": t(d["runs"], pc, 0.794), "B": s["B_median_t03_reduction"],
                     "saved": s["B_median_t03_minutes_saved"], "t01_delta": s["G4_median_t01_delta_min"],
                     "over_max": s["G1_max_over_all_runs"], "starts_max": s["G2_max_starts_per_day"],
                     "guards_failing": [g for g, ok in s["G"].items() if not ok], "pass": s["class_pass"]})
    with open(os.path.join(a.dir, "table.json"), "w", encoding="utf-8") as f: json.dump(rows, f, indent=1)

    fig, ax = plt.subplots(1, 2, figsize=(13, 5))
    x = range(len(rows)); w = 0.27
    ax[0].bar([i - w for i in x], [r["t03_007"] for r in rows], w, label="0.07 (thermo-nova, TASK-1655 data)", color="#bbbbbb")
    ax[0].bar(list(x), [r["t03_0063"] for r in rows], w, label="0.063 (firmware today)", color="#3b6fb6")
    ax[0].bar([i + w for i in x], [r["t03_005"] for r in rows], w, label="0.05 (variant)", color="#e08a2c")
    ax[0].set_xticks(list(x)); ax[0].set_xticklabels([r["class"] for r in rows], rotation=35, ha="right")
    ax[0].set_ylabel("median minutes to within 0.3 K of target"); ax[0].set_title("Warm-up time per plant class")
    ax[0].legend(fontsize=8)
    ax[1].bar(list(x), [100 * r["B"] for r in rows], 0.5, color="#e08a2c")
    ax[1].axhline(10, color="k", ls="--", lw=1, label="pre-registered benefit threshold 10 %")
    ax[1].set_xticks(list(x)); ax[1].set_xticklabels([r["class"] for r in rows], rotation=35, ha="right")
    ax[1].set_ylabel("median paired reduction of t03, 0.05 vs 0.063 (%)"); ax[1].set_title("Gain of 0.05 over 0.063")
    ax[1].legend(fontsize=8)
    fig.suptitle("SAT KD factor 0.05 vs 0.063, warm-up simulation (TASK-1784; simulation only)")
    fig.tight_layout(); fig.savefig(os.path.join(a.dir, "kd_005_vs_0063.png"), dpi=110)
    for r in rows: print(r)


if __name__ == "__main__":
    main()
