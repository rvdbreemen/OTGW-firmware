"""TASK-1519: export simulation results to importable files and draw the charts.

Usage:
  python scripts/sat_curve_selftune_report.py --round 1 --results <study_results.json> --weather <weather json> --out docs/sat/sim-1519
Writes:
  <out>/data/round<N>_runs.csv        one row per simulated winter (all parameters and scores)
  <out>/data/round<N>_summary.csv     mean scores per tuner setting (all houses / realistic houses)
  <out>/data/round<N>_trace_<name>.csv hour-by-hour series of the selected runs (rerunnable: the parameters are in the name file)
  <out>/data/round<N>_selected.json   the parameters of every selected run, to rerun it with the study script
  <out>/round<N>_*.png                charts
Load in Python:  import pandas as pd; df = pd.read_csv("docs/sat/sim-1519/data/round1_runs.csv")
"""
import csv, json, os, statistics, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def arg(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default

ROUND = int(arg("--round", "1")); RES = arg("--results"); WEATHER = arg("--weather"); OUT = arg("--out", "docs/sat/sim-1519")
DATA = os.path.join(OUT, "data"); os.makedirs(DATA, exist_ok=True)
res = json.load(open(RES))

# ---------------- runs CSV + summary
keys = sorted({k for r in res for k in r.keys()})
with open(os.path.join(DATA, f"round{ROUND}_runs.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); [w.writerow(r) for r in res]

def realistic(r):
    if "kr" in r and ROUND == 1: return r["kr"] == 1.0                      # round 1: only K/H 1.0 is inside 1.6-2.4
    return 1.5 <= (r.get("cbest") or r.get("c_best_season") or 0) <= 2.45

def comfort_key(r):
    return r.get("out_band_min_d", r.get("under_min_per_day", 0) + r.get("over_min_per_day", 0))

by = {}
for r in res: by.setdefault(r["spec"], []).append(r)
rows = []
for spec, rs in by.items():
    for scope, sel in (("all", rs), ("realistic", [r for r in rs if realistic(r)])):
        if not sel: continue
        conv = [r["conv_day"] for r in sel if r["conv_day"] is not None]
        rows.append({"spec": spec, "scope": scope, "n": len(sel),
                     "rms": round(statistics.fmean(r["rms"] for r in sel), 4),
                     "minutes_out_of_band_per_day": round(statistics.fmean(comfort_key(r) for r in sel), 2),
                     "writes": round(statistics.fmean(r["writes"] for r in sel), 2),
                     "flips": round(statistics.fmean(r["flips"] for r in sel), 2),
                     "wrong_way": round(statistics.fmean(r["wrong"] for r in sel), 2),
                     "gas_pct": round(statistics.fmean(r["gas_pct"] for r in sel), 2) if "gas_pct" in sel[0] else "",
                     "converged_pct": round(100 * len(conv) / len(sel), 1),
                     "days_to_converge": round(statistics.fmean(conv), 1) if conv else ""})
with open(os.path.join(DATA, f"round{ROUND}_summary.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); [w.writerow(r) for r in rows]

# ---------------- Pareto chart: comfort vs stability per tuner setting (realistic houses)
real = [r for r in rows if r["scope"] == "realistic"]
fig, ax = plt.subplots(figsize=(10, 6.5))
for r in real:
    s = r["spec"]; kind = s.split()[0]
    color = {"none": "black", "H": "tab:orange", "D": "tab:blue", "P": "tab:green"}.get(kind, "gray")
    if "undo" in s: color = "tab:red"
    if "fast" in s: color = "tab:purple"
    ax.scatter(r["writes"], r["rms"], s=20 + 12 * r["flips"], c=color, alpha=0.7, edgecolors="k", linewidths=0.3)
for r in sorted(real, key=lambda x: x["rms"])[:6] + [x for x in real if x["spec"] == "none"]:
    ax.annotate(r["spec"], (r["writes"], r["rms"]), fontsize=7, xytext=(4, 3), textcoords="offset points")
ax.set_xlabel("coefficient changes per winter (flash writes)"); ax.set_ylabel("room error RMS (C), lower is better")
ax.set_title(f"TASK-1519 round {ROUND}: comfort vs stability per tuner setting (realistic houses)\n"
             "bubble size = direction flips; orange hourly, blue daily, red daily+undo, green proportional, purple fast lane, black none")
ax.grid(alpha=0.3); fig.tight_layout(); fig.savefig(os.path.join(OUT, f"round{ROUND}_pareto.png"), dpi=110); plt.close(fig)

# ---------------- pick runs to trace
pool = [r for r in res if realistic(r)]
def pick(label, cond, key, reverse=False):
    cand = [r for r in pool if cond(r)]
    return (label, sorted(cand, key=key, reverse=reverse)[0]) if cand else None
best_spec = min(real, key=lambda r: r["rms"] + 0.002 * r["writes"])["spec"]          # comfort with a penalty on churn
picks = [p for p in [
    pick("baseline_no_tuner_curve_1_too_low", lambda r: r["spec"] == "none" and r["c_off"] == -1.0, key=lambda r: r["seed"]),
    pick("best_balanced_" + best_spec.replace(" ", "_"), lambda r: r["spec"] == best_spec and r["c_off"] == -1.0, key=lambda r: r["rms"]),
    pick("most_flipflops", lambda r: True, key=lambda r: r["flips"], reverse=True),
    pick("most_wrong_way_steps", lambda r: True, key=lambda r: r["wrong"], reverse=True),
    pick("worst_comfort_with_tuner", lambda r: r["spec"] != "none", key=lambda r: r["rms"], reverse=True),
] if p]
json.dump([{k: v for k, v in r.items()} | {"label": lab} for lab, r in picks], open(os.path.join(DATA, f"round{ROUND}_selected.json"), "w"), indent=1)

# ---------------- rerun the picks with an hourly trace and plot them
import sat_curve_selftune_study as base
base._init(WEATHER)
if ROUND == 1:
    import sat_curve_selftune_study as mod
    def spec_of(name):
        if name == "none": return ("none",)
        p = name.split()
        if p[0] == "H": return ("H", int(p[1][1:-1]), int(p[2][1:]), float(p[3][3:]), float(p[4][2:]))
        return ("D", int(p[1][:-1]), float(p[2][3:]), float(p[3][2:]), "undo" in name)
    def rerun(r): return mod.run((spec_of(r["spec"]), r["tau"], r["kr"], r["area"], r["c_off"], r["seed"], "w", True))["trace"]
    def best_c(r, to): return base.best_coeff(r["kr"], to)
elif ROUND == 2:
    import sat_curve_selftune_study2 as mod
    def rerun(r): return mod.run((SPEC2[r["spec"]], r["tau"], r["kr"], r["area"], r["c_off"], r["seed"], True))["trace"]
    def best_c(r, to): return base.best_coeff(r["kr"], to)
else:
    import sat_curve_selftune_study3 as mod
    def rerun(r): return mod.run((SPEC2[r["spec"]], r["era"], r["mass"], r["kr"], r["c_off"], r["seed"], True))["trace"]
    def best_c(r, to):
        Ca, Cw, Gao, Gwo, Heff, K = mod.house(r["era"], r["mass"], r["kr"]); return mod.best_coeff2(Heff, K, to)
if ROUND in (2, 3):
    SPEC2 = {}
    import itertools
    import sat_curve_selftune_study2 as s2
    for spec in json.load(open(arg("--specs"))): SPEC2[s2.make(tuple(spec)).name] = tuple(spec)

for lab, r in picks:
    tr = rerun(r)
    fn = os.path.join(DATA, f"round{ROUND}_trace_{lab}.csv")
    cols = ["hour", "outside_c", "sun_w_m2", "room_c"] + (["wall_c"] if ROUND == 3 else []) + ["setpoint_c", "curve_c", "coefficient", "household_gain_w"]
    with open(fn, "w", newline="") as f:
        w = csv.writer(f); w.writerow(cols); w.writerows(tr)
    hrs = [t[0] / 24 for t in tr]; tout = [t[1] for t in tr]; sun = [t[2] for t in tr]; room = [t[3] for t in tr]
    coef = [t[-2] for t in tr]
    daily_best = []
    for i in range(len(tr)):
        j0 = max(0, i - 23); to = statistics.fmean(tout[j0:i + 1]); b = best_c(r, to); daily_best.append(b if b else float("nan"))
    fig, axs = plt.subplots(3, 1, figsize=(12, 9), sharex=True)
    axs[0].plot(hrs, coef, lw=1.4, label="coefficient used"); axs[0].plot(hrs, daily_best, lw=0.8, alpha=0.7, label="best coefficient for the last 24 h of weather")
    axs[0].set_ylabel("curve coefficient"); axs[0].legend(fontsize=8); axs[0].grid(alpha=0.3)
    axs[1].plot(hrs, room, lw=0.6, label="room")
    if ROUND == 3: axs[1].plot(hrs, [t[4] for t in tr], lw=0.8, label="wall (thermal battery)")
    axs[1].axhline(20.0, color="k", lw=0.8); axs[1].axhspan(19.7, 20.3, color="green", alpha=0.08, label="+-0.3 C band")
    axs[1].set_ylabel("C"); axs[1].legend(fontsize=8); axs[1].grid(alpha=0.3)
    axs[2].plot(hrs, tout, lw=0.6, label="outside (real 2025-26)"); ax2 = axs[2].twinx(); ax2.fill_between(hrs, sun, color="gold", alpha=0.3, label="sun W/m2")
    axs[2].set_ylabel("outside C"); ax2.set_ylabel("sun W/m2"); axs[2].set_xlabel("day of winter (1 Nov 2025 = 0)"); axs[2].grid(alpha=0.3)
    house = f"tau {r.get('tau', '')}h K/H {r.get('kr')}" if ROUND < 3 else f"era {r['era']} {r['mass']} kr {r['kr']}"
    fig.suptitle(f"round {ROUND}: {lab}\n{r['spec']} | {house} | start offset {r['c_off']} | rms {r['rms']:.3f} C, writes {r['writes']}, flips {r['flips']}, wrong-way {r['wrong']}", fontsize=9)
    fig.tight_layout(); fig.savefig(os.path.join(OUT, f"round{ROUND}_{lab}.png"), dpi=100); plt.close(fig)
    print("chart", lab)
print("written", DATA)
