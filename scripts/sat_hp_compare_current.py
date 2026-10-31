"""TASK-1527: compare every heat-pump control strategy with SAT's current control (Robert 2026-10-31: "Also add a
comparison to our current control strategy").

Current control for heat pumps = SAT PWM with the 1800 s cycle (iHpCycleSeconds default, SATcontrol.ino:440-446,
entries 40/50). Every strategy is paired with the current control on the same house, season and seed, and the
change is reported as: starts per day (and the % change), room error (RMS for the radiator plant, mean |error| for
the underfloor plant), minutes per day outside +-0.3 C, and energy (heat-pump electricity), as % of current.
Sources: round16_hp_dm_runs.json (radiators: hp30 / hp60 / hp100, full seasons) and round15_hp_season_runs.json
(on/off heat pump on underfloor heating, full seasons).
Run: python scripts/sat_hp_compare_current.py  -> prints a Markdown table, writes round16_vs_current.png
"""
import collections, json, os, statistics as st, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_sim_plots as sp

HP = sp.HP

def rows_radiators():
    R = sp.load("round16_hp_dm_runs.json")
    out = []
    if not R: return out
    key = lambda w, r: (w, r["source"], r["era"], r["mass"], r["seed"])
    cur = {key(w, r): r for w, rs in R.items() for r in rs if r["mode"] == "sat" and r["par"] == 1800}
    g = collections.defaultdict(list)
    for w, rs in R.items():
        for r in rs: g[(r["source"], r["mode"], r["par"])].append((cur[key(w, r)], r))
    for (src, mode, par), L in sorted(g.items(), key=lambda kv: (kv[0][0], kv[0][1] != "sat", kv[0][1], kv[0][2])):
        c_st = st.mean(c["starts_per_day"] for c, r in L); n_st = st.mean(r["starts_per_day"] for c, r in L)
        out.append({"plant": {"hp30": "radiators, modulating 30 %", "hp60": "radiators, modulating 60 %", "hp100": "radiators, on/off"}[src],
                    "strategy": ("current: SAT PWM 30 min" if (mode, par) == ("sat", 1800) else sp.label(mode, par) if mode != "sat" else f"SAT PWM {par // 60} min"),
                    "fam": sp.family(mode, par), "starts": n_st, "d_starts_pct": 100 * (n_st / c_st - 1) if c_st else 0,
                    "err": st.mean(r["rms"] for c, r in L), "d_err": st.mean(r["rms"] - c["rms"] for c, r in L),
                    "out": st.mean(r["out_band_min_d"] for c, r in L), "d_out": st.mean(r["out_band_min_d"] - c["out_band_min_d"] for c, r in L),
                    "energy_pct": 100 * st.mean(r["fuel_J"] / c["fuel_J"] for c, r in L) - 100, "err_kind": "RMS"})
    return out

def rows_underfloor():
    R = sp.load("round15_hp_season_runs.json")
    out = []
    if not R: return out
    key = lambda w, r: (w, r["era"], r["screed"], r["seed"])
    cur = {key(w, r): r for w, rs in R.items() for r in rs if r.get("plant") == "onoff_ufh" and r["variant"] == "pwm" and r["par"] == 30}
    g = collections.defaultdict(list)
    for w, rs in R.items():
        for r in rs:
            if r.get("plant") == "onoff_ufh": g[(r["screed"] > 5e6, r["variant"], r["par"])].append((cur[key(w, r)], r))
    for (heavy, v, p), L in sorted(g.items(), key=lambda kv: (not kv[0][0], kv[0][1] != "pwm", kv[0][1], kv[0][2])):
        c_st = st.mean(c["starts_per_day"] for c, r in L); n_st = st.mean(r["starts_per_day"] for c, r in L)
        out.append({"plant": f"underfloor, on/off, {'heavy' if heavy else 'light'} floor",
                    "strategy": "current: SAT PWM 30 min" if (v, p) == ("pwm", 30) else sp.label(v, p), "fam": sp.family(v, p),
                    "starts": n_st, "d_starts_pct": 100 * (n_st / c_st - 1) if c_st else 0,
                    "err": st.mean(r["mean_abs_err"] for c, r in L), "d_err": st.mean(r["mean_abs_err"] - c["mean_abs_err"] for c, r in L),
                    "out": st.mean(r["out_band_min_d"] for c, r in L), "d_out": st.mean(r["out_band_min_d"] - c["out_band_min_d"] for c, r in L),
                    "energy_pct": 100 * st.mean(r["elec_kwh"] / c["elec_kwh"] for c, r in L) - 100, "err_kind": "mean abs"})
    return out

def table(rows):
    print("| plant | strategy | starts/day | vs current | room error (C) | vs current | min/day outside +-0.3 C | vs current | energy vs current |")
    print("|---|---|---|---|---|---|---|---|---|")
    for r in rows:
        print(f"| {r['plant']} | {r['strategy']} | {r['starts']:.1f} | {r['d_starts_pct']:+.0f} % | {r['err']:.3f} ({r['err_kind']}) | {r['d_err']:+.3f} | {r['out']:.0f} | {r['d_out']:+.0f} | {r['energy_pct']:+.1f} % |")

def chart(rows):
    import matplotlib.pyplot as plt
    plants = list(dict.fromkeys(r["plant"] for r in rows))
    f, grid = plt.subplots(2, 3, figsize=(15, 9), facecolor=sp.SURFACE)
    axs = [grid[0][0], grid[0][1], grid[0][2], grid[1][0], grid[1][1]]
    grid[1][2].axis("off")
    for ax, pl in zip(axs, plants):
        rs = [r for r in rows if r["plant"] == pl and not r["strategy"].startswith("current")]
        rs.sort(key=lambda r: r["d_starts_pct"])
        y = range(len(rs))
        lo = min(min(r["d_starts_pct"] for r in rs), 0); hi = max(max(r["d_starts_pct"] for r in rs), 0)
        for i, r in zip(y, rs):
            c, mk, _ = sp.FAM[r["fam"]]
            ax.barh([i], [r["d_starts_pct"]], color=c, height=0.6)
            x_lab = max(r["d_starts_pct"], 0)          # label right of the bar end or of the zero line
            ax.annotate(f"comfort {r['d_err']:+.2f} C", (x_lab, i), xytext=(4, 0), textcoords="offset points",
                        ha="left", va="center", fontsize=7, color=sp.INK)
        ax.set_xlim(lo - 5, hi + 45)
        ax.set_yticks(list(y)); ax.set_yticklabels([r["strategy"] for r in rs], fontsize=7)
        ax.axvline(0, color=sp.INK2, linewidth=0.8)
        sp.style(ax, pl, "starts per day vs today's SAT PWM 30 min (%)", "")
    sp.save(f, os.path.join(HP, "round16_vs_current.png"),
            "Change against today's heat-pump control (SAT PWM 30 min), paired per house, season and seed. Bar = change in starts; label = change in room error "
            "(negative = better). Full seasons Oct-Apr. Model, indicative.")

if __name__ == "__main__":
    rows = rows_radiators() + rows_underfloor()
    table(rows); chart(rows)
