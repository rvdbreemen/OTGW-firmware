#!/usr/bin/env python3
"""TASK-1883 AC2: EXPLORATORY offline heat-loss and heating-curve-coefficient estimate from a capture.

EXPLORATORY. Every number this script prints is a research estimate for one house and one capture.
It is not a recommendation for any unit and is never written to a device.

Input: one or more web-UI graph exports (otgw-graph-*.csv, columns epoch_ms, flow, ret, chSet, roomSet,
room, outside, mod, flame, dhw_on, ch). Several files are concatenated in time order (contiguous
captures of one house). Downloaded data is untrusted: run with `python -I` and pass the paths as arguments.

Method (after VTherm tpi_identification.py:98-198, see docs/sat/vtherm-comparison-2026-11-13.md lesson 4):
  per time bin b (default 20 min):  dTin/dt = g * (Tout - Tin) + B * u + q
    dTin/dt  room temperature slope over the bin (K/h, least-squares slope of the samples)
    Tout-Tin mean outside minus room temperature over the bin (K)
    u        mean emitter drive over the bin: (flow - room) while CH is enabled, else 0 (K)
    q        constant gains (sun, people, appliances) as a room warming rate (K/h)
  Exclusions: bins with hot water (dhw_on), bins with less than 80 % sample coverage.
  Guards: 0 < g < 1 /h, B > 0, condition number of the design matrix < 1000, >= 40 bins, >= 40 flame cycles.
  70/30 chronological split: fit on the first 70 %, report the hold-out RMSE against a zero-slope model.
  Uncertainty: moving-block bootstrap over bins (block = 3 bins, 1 h by default), refit, percentile 95 % CI.

From (g, B, q) the steady state at room temperature T* and outside temperature To needs the flow temperature
    Tflow_req = T* + (g * (T* - To) - q) / B
and the coefficient c* is the value for which the firmware heating curve (SATcontrol.ino satCalcHeatingCurve,
radiators base offset 27.2) gives that flow temperature:
    curve(c) = 27.2 + c/4 * (4 (T* - 20) + 0.03 (To - 20)^2 - 0.4 (To - 20))   ->   c* = 4 (Tflow_req - 27.2) / cv

Only numpy is needed (the comparison in heat-loss-identification-2026-11-13.md explains the choice).
Run: python -I heat_loss_id.py [--bin-min 20] [--block 3] [--boot 2000] [--tstar 19.0] [--no-q] FILE [FILE ...]
"""
import argparse
import csv
import json
import math
import sys

import numpy as np

BASE_RAD = 27.2   # SAT_HC_BASE_OFFSET_RAD (SATcontrol.ino satGetBaseOffset)


def load(paths):
    rows = []
    for p in paths:
        with open(p, newline="", encoding="utf-8") as f:
            for r in csv.DictReader(f):
                rows.append(r)
    def num(v):
        try:
            return float(v) if v != "" else math.nan
        except ValueError:
            return math.nan
    cols = ["flow", "ret", "chSet", "roomSet", "room", "outside", "mod", "flame", "dhw_on", "ch"]
    t = np.array([int(r["epoch_ms"]) / 1000.0 for r in rows])
    d = {c: np.array([num(r.get(c, "")) for r in rows]) for c in cols}
    order = np.argsort(t, kind="stable")
    t = t[order]
    d = {c: v[order] for c, v in d.items()}
    return t, d


def flame_cycles(d):
    f = np.nan_to_num(d["flame"], nan=0.0) > 0.5
    return int(np.sum(~f[:-1] & f[1:]))


def bins(t, d, bin_s, sample_s=5.0, min_cov=0.8):
    t0 = t[0]
    idx = ((t - t0) // bin_s).astype(int)
    out = []
    for b in range(idx.max() + 1):
        m = idx == b
        n = int(m.sum())
        if n < min_cov * bin_s / sample_s:
            continue
        if np.nansum(d["dhw_on"][m]) > 0:
            continue
        room, out_t, flow, ch = d["room"][m], d["outside"][m], d["flow"][m], d["ch"][m]
        tt = t[m]
        ok = np.isfinite(room) & np.isfinite(out_t) & np.isfinite(flow) & np.isfinite(ch)
        if ok.sum() < min_cov * bin_s / sample_s:
            continue
        th = (tt[ok] - tt[ok].mean()) / 3600.0
        slope = float(np.polyfit(th, room[ok], 1)[0])            # K/h
        tin = float(room[ok].mean())
        u = float(np.mean(np.where(ch[ok] > 0.5, flow[ok] - room[ok], 0.0)))
        out.append(dict(t=float(tt[0]), slope=slope, dTo=float(out_t[ok].mean()) - tin, u=u, tin=tin,
                        tout=float(out_t[ok].mean()), duty=float(np.nanmean(d["flame"][m]))))
    return out


def fit(B, with_q=True):
    cols = [[b["dTo"] for b in B], [b["u"] for b in B]] + ([np.ones(len(B))] if with_q else [])
    X = np.column_stack(cols)
    y = np.array([b["slope"] for b in B])
    coef, *_ = np.linalg.lstsq(X, y, rcond=None)
    return coef, X, y


def full(c):
    """(g, B, q) from a fit with or without q."""
    return (float(c[0]), float(c[1]), float(c[2]) if len(c) > 2 else 0.0)


def curve_value(tstar, to):
    d = to - 20.0
    return 4.0 * (tstar - 20.0) + 0.03 * d * d - 0.4 * d


def coeff_star(coef, tstar, to):
    g, b, q = coef
    if b <= 0:
        return math.nan, math.nan
    tflow = tstar + (g * (tstar - to) - q) / b
    return 4.0 * (tflow - BASE_RAD) / curve_value(tstar, to), tflow


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--bin-min", type=float, default=20.0)
    ap.add_argument("--block", type=int, default=3)
    ap.add_argument("--boot", type=int, default=2000)
    ap.add_argument("--tstar", type=float, default=19.0)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--no-q", action="store_true", help="variant: q = 0 (no constant gains; also drops sun and people)")
    a = ap.parse_args()

    t, d = load(a.files)
    hours = (t[-1] - t[0]) / 3600.0
    cycles = flame_cycles(d)
    B = bins(t, d, a.bin_min * 60.0)
    res = dict(label="EXPLORATORY", files=[p.replace("\\", "/").split("/")[-1] for p in a.files],
               hours=round(hours, 2), flame_cycles=cycles, bins=len(B), bin_min=a.bin_min,
               tout_range=[round(min(b["tout"] for b in B), 1), round(max(b["tout"] for b in B), 1)] if B else None)
    if len(B) < 6:
        res["verdict"] = "too few bins to fit"
        print(json.dumps(res, indent=1))
        return 0
    wq = not a.no_q
    coef, X, y = fit(B, wq)
    cond = float(np.linalg.cond(X))
    # 70/30 chronological split
    k = int(round(0.7 * len(B)))
    ctr, Xtr, ytr = fit(B[:k], wq)
    Xte, yte = X[k:], y[k:]
    rmse_fit = float(np.sqrt(np.mean((Xte @ ctr - yte) ** 2)))
    rmse_zero = float(np.sqrt(np.mean(yte ** 2)))
    resid = y - X @ coef
    r2 = float(1 - np.sum(resid ** 2) / np.sum((y - y.mean()) ** 2))
    guards = dict(g_in_0_1=bool(0 < coef[0] < 1), B_pos=bool(coef[1] > 0), cond_lt_1000=bool(cond < 1000),
                  bins_ge_40=len(B) >= 40, cycles_ge_40=cycles >= 40,
                  holdout_beats_zero=bool(rmse_fit < rmse_zero))
    # moving-block bootstrap over bins
    rng = np.random.default_rng(a.seed)
    nb = len(B)
    blk = max(1, a.block)
    starts = np.arange(0, nb - blk + 1)
    tos = [5.0, 0.0, -5.0]
    boot = {"g": [], "B": [], "q": [], **{f"c@{to:g}": [] for to in tos}}
    allc = {f"c@{to:g}": [] for to in tos}   # every resample with B > 0, guards or not (unconditional)
    rejected = 0
    for _ in range(a.boot):
        pick = []
        while len(pick) < nb:
            s = int(rng.choice(starts))
            pick.extend(range(s, s + blk))
        pick = pick[:nb]
        cb = full(np.linalg.lstsq(X[pick], y[pick], rcond=None)[0])
        if cb[1] > 0:
            for to in tos:
                allc[f"c@{to:g}"].append(coeff_star(cb, a.tstar, to)[0])
        if not (0 < cb[0] < 1 and cb[1] > 0):
            rejected += 1                       # outside the physical guards: counted, not used
            continue
        boot["g"].append(cb[0]); boot["B"].append(cb[1]); boot["q"].append(cb[2])
        for to in tos:
            boot[f"c@{to:g}"].append(coeff_star(cb, a.tstar, to)[0])
    ci = lambda v: [round(float(np.percentile(v, 2.5)), 3), round(float(np.percentile(v, 97.5)), 3)] if v else None
    coef = full(coef)
    res.update(model="g, B (q = 0)" if a.no_q else "g, B, q",
        g_per_h=round(float(coef[0]), 4), B_per_h=round(float(coef[1]), 4), q_K_per_h=round(float(coef[2]), 3),
        g_over_B=round(float(coef[0] / coef[1]), 3) if coef[1] > 0 else None,
        tau_h=round(float(1 / coef[0]), 1) if coef[0] > 0 else None,
        r2=round(r2, 3), cond=round(cond, 1), holdout_rmse=round(rmse_fit, 4), holdout_rmse_zero=round(rmse_zero, 4),
        guards=guards, boot_n=a.boot, boot_rejected_by_guards=rejected,
        note="ci95_guarded uses only the resamples inside the guards (conditional); ci95_unconditional every resample with B > 0",
        ci95=dict(g=ci(boot["g"]), B=ci(boot["B"]), q=ci(boot["q"])), tstar=a.tstar, coefficient={})
    for to in tos:
        c, tf = coeff_star(coef, a.tstar, to)
        res["coefficient"][f"Tout={to:g}"] = dict(c_star=round(c, 3), tflow_req=round(tf, 1),
                                                  ci95_guarded=ci(boot[f"c@{to:g}"]),
                                                  ci95_unconditional=ci(allc[f"c@{to:g}"]),
                                                  n_unconditional=len(allc[f"c@{to:g}"]))
    res["verdict"] = ("all guards pass" if all(guards.values())
                      else "guards failed: " + ", ".join(k for k, v in guards.items() if not v))
    print(json.dumps(res, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
