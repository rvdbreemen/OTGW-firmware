"""TASK-1519 round 6: can SAT measure the house itself?
(a) Thermal time constant from a night cool-down: start from the heated steady state of the two-node house (round 3
    model), switch the heat off for 4 hours with no sun and no household gains, and fit the slope of ln(Tair - Tout)
    over hours 1-4. Compare with the true slow time constant (eigenvalues of the two-node system) and with Sergeant D's
    60-72 h (entry 168).
(b) Oscillation: count sign changes per hour of (room - target) and the swing amplitude over a day, on round-3 runs of
    a stable house and of the light pre-1965 house where the PID oscillates.
Run: python scripts/sat_curve_selftune_study6.py --weather <json>
"""
import math, os, statistics, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study3 as s3

def true_taus(era, mass):
    Ca, Cw, Gao, Gwo, Heff, K = s3.house(era, mass, 1.0)
    G = s3.G_AW
    A = np.array([[-(G + Gao) / Ca, G / Ca], [G / Cw, -(G + Gwo) / Cw]])
    ev = sorted(-1.0 / np.linalg.eigvals(A).real / 3600.0)
    return ev[0], ev[1]                                   # fast, slow (hours)

def cooldown_tau(era, mass, tout=3.0, hours=4.0):
    Ca, Cw, Gao, Gwo, Heff, K = s3.house(era, mass, 1.0)
    Ta = 20.0; Tw = (s3.G_AW * Ta + Gwo * tout) / (s3.G_AW + Gwo)   # heated steady state
    dt = 60.0; pts = []
    for m in range(int(hours * 60)):
        qaw = s3.G_AW * (Ta - Tw)
        Ta += (-qaw - Gao * (Ta - tout)) * dt / Ca
        Tw += (qaw - Gwo * (Tw - tout)) * dt / Cw
        if m >= 60: pts.append((m / 60.0, math.log(Ta - tout)))
    x = [p[0] for p in pts]; y = [p[1] for p in pts]
    slope = np.polyfit(x, y, 1)[0]
    return -1.0 / slope, 20.0 - Ta

def oscillation(trace):
    """sign changes per hour of (room - 20) and the median daily swing, over the scored days."""
    rows = [t for t in trace if t[0] >= 14 * 24]
    e = [t[3] - 20.0 for t in rows]
    flips = sum(1 for i in range(1, len(e)) if (e[i] > 0) != (e[i - 1] > 0))
    days = {}
    for t in rows: days.setdefault(int(t[0] // 24), []).append(t[3])
    swing = statistics.median(max(v) - min(v) for v in days.values())
    return flips / len(rows), swing

if __name__ == "__main__":
    w = sys.argv[sys.argv.index("--weather") + 1]
    print("(a) time constant from a 4 h night cool-down (heat off, no sun, outside 3 C)")
    print(f"{'era':12s} {'mass':6s} {'true fast h':>11s} {'true slow h':>11s} {'estimated h':>11s} {'room drop C':>11s}")
    for era in s3.ERAS:
        for mass in ("heavy", "light"):
            f, s = true_taus(era, mass); est, drop = cooldown_tau(era, mass)
            print(f"{era:12s} {mass:6s} {f:11.1f} {s:11.1f} {est:11.1f} {drop:11.2f}")
    print("\n(b) oscillation detector on round-3 runs (no tuner, right curve)")
    base._init(w)
    for era, mass in (("4_1992-2012", "heavy"), ("6_BENG2021", "heavy"), ("1_pre1965", "heavy"), ("1_pre1965", "light")):
        tr = s3.run((("none",), era, mass, 1.0, 0.0, 1, True))["trace"]
        fph, swing = oscillation(tr)
        print(f"  {era:12s} {mass:6s} sign changes per hour {fph:5.2f}   median daily swing {swing:5.2f} C")
