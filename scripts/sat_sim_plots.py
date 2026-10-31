"""Charts for the TASK-1519 / TASK-1527 simulation rounds (Robert 2026-10-31: "Maak ook visualisaties van alle
simulaties die je uitvoert"). Reads the stored run data, writes PNGs next to the reports.

Palette: the validated reference categorical palette (dataviz skill, references/palette.md, light mode), checked
with validate_palette.js: all checks pass; three slots are below 3:1 contrast, so every point carries a direct
label and a marker shape per family (colour is never the only cue).
Run: python scripts/sat_sim_plots.py [--only hp|tune]
"""
import collections, glob, json, os, statistics as st, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HP = os.path.join(ROOT, "docs", "sat", "sim-hp-lowload")
TUNE = os.path.join(ROOT, "docs", "sat", "sim-1519")
SURFACE, INK, INK2, GRID = "#fcfcfb", "#1f1f1d", "#5f5e58", "#e4e3de"
FAM = {  # fixed categorical order, never cycled
    "today": ("#2a78d6", "o", "SAT PWM today"),
    "pwm120": ("#eb6834", "s", "SAT PWM 120 min"),
    "tp": ("#1baf7a", "D", "time-proportional PWM"),
    "dm": ("#eda100", "^", "degree minutes"),
    "rh": ("#e87ba4", "v", "room hysteresis"),
    "own": ("#008300", "h", "heat pump's own control"),
    "mr": ("#6250d6", "X", "fixed long runs"),
}

def style(ax, title, xl, yl):
    ax.set_facecolor(SURFACE)
    for s in ("top", "right"): ax.spines[s].set_visible(False)
    for s in ("left", "bottom"): ax.spines[s].set_color(GRID)
    ax.tick_params(colors=INK2, labelsize=8)
    ax.grid(True, color=GRID, linewidth=0.6); ax.set_axisbelow(True)
    ax.set_title(title, color=INK, fontsize=10, loc="left")
    ax.set_xlabel(xl, color=INK2, fontsize=8); ax.set_ylabel(yl, color=INK2, fontsize=8)

def fig(n=1, w=11, h=5):
    f, axs = plt.subplots(1, n, figsize=(w, h), facecolor=SURFACE)
    return f, (axs if n > 1 else [axs])

def save(f, path, note):
    f.text(0.01, 0.01, note, color=INK2, fontsize=7)
    f.tight_layout(rect=(0, 0.04, 1, 1)); f.savefig(path, dpi=130, facecolor=SURFACE); plt.close(f); print("wrote", os.path.relpath(path, ROOT))

def family(v, p):
    if v == "pwm": return "pwm120" if p >= 120 else "today"
    return {"tp": "tp", "dm": "dm", "rh": "rh", "own": "own", "mr": "mr", "sat": "today"}[v]

def label(v, p):
    if v in ("pwm", "sat"): return f"PWM {int(p) // 60 if p > 300 else int(p)} min"
    if v == "tp": return f"TP {int(p)}"
    if v == "dm": return f"DM -{int(p)}"
    if v == "rh": return f"RH {p}"
    if v == "mr": return f"run {int(p)}"
    return "own"

def scatter(ax, pts):
    """pts: list of (family, label, x=starts/day, y=error). Fewer starts and lower error = lower left."""
    seen = set()
    for fam, lab, x, y in pts:
        c, mk, name = FAM[fam]
        ax.scatter([x], [y], s=70, color=c, marker=mk, edgecolors=SURFACE, linewidths=1.5, zorder=3, label=None if fam in seen else name)
        seen.add(fam)
    ax.set_xscale("log")
    ax.set_xticks([1, 2, 5, 10, 20, 50]); ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter()); ax.minorticks_off()
    ax.figure.canvas.draw()
    placed = []   # label boxes in display coordinates; try offsets until no overlap
    for fam, lab, x, y in sorted(pts, key=lambda t: (t[2], t[3])):
        for dx, dy in ((6, 4), (6, -11), (-6, 4), (-6, -11), (6, 14), (6, -21)):
            t = ax.annotate(lab, (x, y), xytext=(dx, dy), textcoords="offset points", fontsize=7, color=INK, ha="left" if dx > 0 else "right")
            bb = t.get_window_extent(renderer=ax.figure.canvas.get_renderer()).expanded(1.05, 1.15)
            if not any(bb.overlaps(o) for o in placed):
                placed.append(bb); break
            t.remove()
    ax.legend(fontsize=7, frameon=False, labelcolor=INK, loc="lower right")

def load(name, folder=HP):
    p = os.path.join(folder, name)
    return json.load(open(p)) if os.path.exists(p) else None

def hp_rounds():
    # round 13 (Nov-Feb) and round 15 (full season): on/off heat pump on underfloor heating
    for name, key, title, out in (("round13_hp_longrun_runs.json", "par", "Nov-Feb, 3 winters", "round13_tradeoff.png"),
                                  ("round15_hp_season_runs.json", "par", "full season Oct-Apr, 3 seasons", "round15_tradeoff.png")):
        R = load(name)
        if not R: continue
        f, axs = fig(2)
        for ax, heavy in zip(axs, (True, False)):
            g = collections.defaultdict(list)
            for w, rs in R.items():
                for r in rs:
                    if r.get("plant", "onoff_ufh") != "onoff_ufh" or "variant" not in r: continue
                    if (r["screed"] > 5e6) == heavy: g[(r["variant"], r[key])].append(r)
            pts = [(family(v, p), label(v, p), st.mean(r["starts_per_day"] for r in L), st.mean(r["mean_abs_err"] for r in L)) for (v, p), L in g.items()]
            scatter(ax, pts)
            style(ax, f"On/off heat pump, underfloor, {'heavy' if heavy else 'light'} floor", "compressor starts per day (log scale)", "mean |room - target| (C)")
        save(f, os.path.join(HP, out), f"TASK-1527 {name[:7]}: {title}; lower left is better. Model, indicative. Source: docs/sat/sim-hp-lowload/{name}")
    # round 14 / 16: heat pumps on radiators, SAT PWM period and control modes
    R = load("round16_hp_dm_runs.json")
    if R:
        f, axs = fig(3, w=14)
        for ax, src, nm in zip(axs, ("hp30", "hp60", "hp100"), ("modulating to 30 %", "modulating to 60 %", "on/off")):
            g = collections.defaultdict(list)
            for w, rs in R.items():
                for r in rs:
                    if r["source"] == src: g[(r["mode"], r["par"])].append(r)
            pts = [(family(m, p), label(m, p) if m != "sat" else f"PWM {p // 60} min", st.mean(r["starts_per_day"] for r in L), st.mean(r["rms"] for r in L)) for (m, p), L in g.items()]
            scatter(ax, pts)
            style(ax, f"Heat pump {nm}, radiators", "starts per day (log scale)", "room RMS error (C)")
        save(f, os.path.join(HP, "round16_tradeoff.png"), "TASK-1527 round16: full seasons Oct-Apr, 5 houses, 2 seeds; lower left is better. Model, indicative. Source: docs/sat/sim-hp-lowload/round16_hp_dm_runs.json")
    R = load("round14_hp_period_runs.json")
    if R:
        f, axs = fig(2)
        for ax, src, nm in zip(axs, ("hp30", "hp60"), ("modulating to 30 %", "modulating to 60 %")):
            g = collections.defaultdict(list)
            for w, rs in R.items():
                for r in rs:
                    if r.get("plant") == "mod_rad" and r["source"] == src: g[r["period_s"]].append(r)
            pts = [("today" if p < 7200 else "pwm120", f"PWM {p // 60} min", st.mean(r["starts_per_day"] for r in L), st.mean(r["rms"] for r in L)) for p, L in g.items()]
            scatter(ax, pts)
            style(ax, f"Heat pump {nm}, radiators: SAT PWM period", "starts per day (log scale)", "room RMS error (C)")
        save(f, os.path.join(HP, "round14_period.png"), "TASK-1527 round14: 30 / 40 / 120 min PWM as SAT does it now, Nov-Feb, 3 winters. Model, indicative. Source: round14_hp_period_runs.json")

def tune_rounds():
    # round 5b: fixed-coefficient sweep, cliff and plateau
    D = load("round5b_calib_sweep.json", os.path.join(TUNE, "data"))
    if D:
        f, axs = fig(3, w=14)
        houses = [("3_1983-92", "heavy", "1983-92"), ("4_1992-2012", "heavy", "1992-2012"), ("5_2015", "heavy", "2015"), ("6_BENG2021", "heavy", "BENG"), ("4_1992-2012", "light", "1992-2012 light")]
        cols = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]
        for ax, src, nm in zip(axs, ("gas", "hp30", "hp60"), ("gas boiler", "heat pump 30 %", "heat pump 60 %")):
            for (e, m, hn), c in zip(houses, cols):
                rs = sorted([r for r in D["res"] if r["source"] == src and r["era"] == e and r["mass"] == m], key=lambda r: r["c"])
                ax.plot([r["c"] for r in rs], [r["out_band_min_d"] for r in rs], color=c, linewidth=2, label=hn)
            ax.axvline(1.8, color=INK2, linewidth=0.8, linestyle=":")
            ax.set_ylim(0, 1000); ax.legend(fontsize=7, frameon=False, labelcolor=INK)
            style(ax, f"{nm}: comfort vs fixed coefficient", "heating curve coefficient", "minutes per day outside +-0.3 C")
        save(f, os.path.join(TUNE, "round5b_cliff_plateau.png"), "TASK-1519 round 5b: radiators sized so 1.8 balances (dotted); cliff below ~1.2, plateau above. Source: data/round5b_calib_sweep.json")
    # round 9: twenty winters, rescue from a curve of 1.0 and flips, 2 vs 3 days
    files = sorted(glob.glob(os.path.join(TUNE, "data", "round9", "*.json")))
    if files:
        f, axs = fig(2, w=14)
        years, resc2, resc3, fl2, fl3 = [], [], [], [], []
        key = lambda r: (r["source"], r["era"], r["mass"], r["start"], r["seed"])
        for fn in files:
            d = json.load(open(fn)); y = os.path.basename(fn)[:7].replace("_", "-"); years.append(y)
            ref = {key(r): r for r in d if r["kind"] == "rule" and r["spec"] == "none"}
            for spec, R1, F1 in (("D 2d up1.0 dn1.0 st0.1", resc2, fl2), ("D 3d up1.0 dn1.0 st0.1", resc3, fl3)):
                v = [r for r in d if r["kind"] == "rule" and r["spec"] == spec and r["source"] == "gas" and r["start"] == 1.0]
                R1.append(st.median(r["out_band_min_d"] - ref[key(r)]["out_band_min_d"] for r in v))
                F1.append(st.mean(r["flips"] for r in d if r["kind"] == "rule" and r["spec"] == spec))
        x = range(len(years)); wdt = 0.4
        axs[0].bar([i - wdt / 2 for i in x], resc2, wdt, color="#2a78d6", label="2-day window")
        axs[0].bar([i + wdt / 2 for i in x], resc3, wdt, color="#eb6834", label="3-day window (chosen)")
        axs[0].set_xticks(list(x)); axs[0].set_xticklabels(years, rotation=60, fontsize=7); axs[0].legend(fontsize=7, frameon=False)
        style(axs[0], "Gas: rescue from a curve of 1.0 per winter (more negative = better)", "winter", "median change, minutes/day outside +-0.3 C")
        axs[1].bar([i - wdt / 2 for i in x], fl2, wdt, color="#2a78d6", label="2-day window")
        axs[1].bar([i + wdt / 2 for i in x], fl3, wdt, color="#eb6834", label="3-day window (chosen)")
        axs[1].set_xticks(list(x)); axs[1].set_xticklabels(years, rotation=60, fontsize=7); axs[1].legend(fontsize=7, frameon=False)
        style(axs[1], "All sources: direction flips per winter (fewer = calmer)", "winter", "mean flips per winter")
        save(f, os.path.join(TUNE, "round9_twenty_winters.png"), "TASK-1519 round 9: the decided self-tune over 20 winters (2005-06 .. 2024-25). Source: data/round9/*.json")
    # round 11: overshoot margin 2.0 vs 3.0
    R = load("round11_runs.json", os.path.join(TUNE, "data"))
    if R:
        f, axs = fig(1, w=8)
        ax = axs[0]; labels, a, b = [], [], []
        k = lambda r: (r["spec"], r["source"], r["era"], r["mass"], r["start"], r["seed"])
        for w in ("2010_11", "2024_25", "2025_26"):
            A = R[f"{w}|2.0"]; B = {k(r): r for r in R[f"{w}|3.0"]}
            for src in ("gas", "hp30", "hp60"):
                v = [r for r in A if r["source"] == src and r["spec"] == "none"]
                labels.append(f"{w.replace('_', '-')}\n{src}"); a.append(st.mean(r["out_band_min_d"] for r in v)); b.append(st.mean(B[k(r)]["out_band_min_d"] for r in v))
        x = range(len(labels)); wdt = 0.4
        ax.bar([i - wdt / 2 for i in x], a, wdt, color="#2a78d6", label="margin 2.0 C (firmware)")
        ax.bar([i + wdt / 2 for i in x], b, wdt, color="#eb6834", label="margin 3.0 C (thermo-nova)")
        ax.set_xticks(list(x)); ax.set_xticklabels(labels, fontsize=7); ax.legend(fontsize=7, frameon=False)
        style(ax, "Overshoot margin, no tuner: minutes per day outside +-0.3 C (lower = better)", "winter / heat source", "minutes per day")
        save(f, os.path.join(TUNE, "round11_margin.png"), "TASK-1519 round 11: calibrated model, indicative (acts through PWM entry). Source: data/round11_runs.json")

def period_sweep():
    """Round 17: SAT PWM period 30..240 min against degree minutes (round 15/16) and time-proportional, per type."""
    R = load("round17_hp_period_sweep_runs.json"); r16 = load("round16_hp_dm_runs.json"); r15 = load("round15_hp_season_runs.json")
    if not (R and r16 and r15): return
    import matplotlib.pyplot as plt
    f, grid = plt.subplots(2, 3, figsize=(15, 9), facecolor=SURFACE)
    panels = [("rad", "hp30", "Radiators, modulating 30 %"), ("rad", "hp60", "Radiators, modulating 60 %"), ("rad", "hp100", "Radiators, on/off"),
              ("ufh", True, "Underfloor, on/off, heavy floor"), ("ufh", False, "Underfloor, on/off, light floor")]
    grid[1][2].axis("off")
    def curve(ax, pts, fam, name):
        c, mk, _ = FAM[fam]
        xs = [x for x, y, l in pts]; ys = [y for x, y, l in pts]
        ax.plot(xs, ys, color=c, linewidth=2, zorder=2)
        ax.scatter(xs, ys, s=50, color=c, marker=mk, edgecolors=SURFACE, linewidths=1.5, zorder=3, label=name)
        for x, y, l in pts: ax.annotate(l, (x, y), xytext=(5, 3), textcoords="offset points", fontsize=7, color=INK)
    for ax, (plant, key, title) in zip([grid[0][0], grid[0][1], grid[0][2], grid[1][0], grid[1][1]], panels):
        if plant == "rad":
            pw = [(st.mean(r["starts_per_day"] for w, rs in R.items() for r in rs if r["plant"] == "rad" and r["source"] == key and r["period_min"] == p),
                   st.mean(r["rms"] for w, rs in R.items() for r in rs if r["plant"] == "rad" and r["source"] == key and r["period_min"] == p), f"{p}") for p in (30, 40, 60, 90, 120, 180, 240)]
            dm = [(st.mean(r["starts_per_day"] for w, rs in r16.items() for r in rs if r["source"] == key and r["mode"] == "dm" and r["par"] == d),
                   st.mean(r["rms"] for w, rs in r16.items() for r in rs if r["source"] == key and r["mode"] == "dm" and r["par"] == d), f"-{d}") for d in (60, 120, 240, 480)]
            curve(ax, pw, "today", "SAT PWM, period in minutes"); curve(ax, dm, "dm", "degree minutes, start at -S")
            yl = "room RMS error (C)"
        else:
            sel = lambda r: r["plant"] == "ufh" and (r["screed"] > 5e6) == key
            pw = [(st.mean(r["starts_per_day"] for w, rs in R.items() for r in rs if sel(r) and r["variant"] == "pwm" and r["period_min"] == p),
                   st.mean(r["mean_abs_err"] for w, rs in R.items() for r in rs if sel(r) and r["variant"] == "pwm" and r["period_min"] == p), f"{p}") for p in (30, 40, 60, 90, 120, 180, 240)]
            tp = [(st.mean(r["starts_per_day"] for w, rs in R.items() for r in rs if sel(r) and r["variant"] == "tp" and r["period_min"] == p),
                   st.mean(r["mean_abs_err"] for w, rs in R.items() for r in rs if sel(r) and r["variant"] == "tp" and r["period_min"] == p), f"{p}") for p in (60, 120, 240)]
            sel15 = lambda r: r.get("plant") == "onoff_ufh" and (r["screed"] > 5e6) == key
            dm = [(st.mean(r["starts_per_day"] for w, rs in r15.items() for r in rs if sel15(r) and r["variant"] == "dm" and r["par"] == d),
                   st.mean(r["mean_abs_err"] for w, rs in r15.items() for r in rs if sel15(r) and r["variant"] == "dm" and r["par"] == d), f"-{d}") for d in (120, 240)]
            curve(ax, pw, "today", "SAT PWM, period in minutes"); curve(ax, tp, "tp", "time-proportional, period"); curve(ax, dm, "dm", "degree minutes")
            yl = "mean |room - target| (C)"
        ax.set_xscale("log"); ax.set_xticks([1, 2, 5, 10, 20, 50]); ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter()); ax.minorticks_off()
        ax.legend(fontsize=7, frameon=False, labelcolor=INK, loc="lower right")
        style(ax, title, "starts per day (log scale)", yl)
    save(f, os.path.join(HP, "round17_period_sweep.png"), "TASK-1527 round 17: SAT's own PWM period extended 30 -> 240 min (labels = minutes) vs degree minutes (rounds 15/16). "
         "Right = more starts, up = worse comfort; today = SAT PWM 30. Full seasons Oct-Apr. Model, indicative.")

if __name__ == "__main__":
    only = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else "all"
    if only in ("all", "hp"): hp_rounds(); period_sweep()
    if only in ("all", "tune"): tune_rounds()
