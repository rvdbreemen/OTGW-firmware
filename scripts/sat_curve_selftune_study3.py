"""TASK-1519 study round 3: two-node house (air + wall thermal battery, Sergeant D entry 168) across Dutch building eras.

House types from docs/sat/sim-1519/house-typology-research.md (derived estimates, labelled there):
era H(W/K) and opaque wall-to-outside conductance G_wo; C = 260 kJ/m2K x 120 m2 (heavy) or 110 (light);
C_air = 10 % of C, C_wall = 90 %; G_aw = 2880 W/K (360 m2 interior surface x 8 W/m2K); G_ao = H - opaque part.
Radiator K = kr * H_eff (H_eff = G_ao + series(G_aw, G_wo)) so the best curve coefficient lands in 1.6-2.4 (entry 166).
Controller, PID, weather and household noise as round 1/2. Tuners: the PropWindow family of round 2 plus waiting
times of 60/72 h after a change (entry 168). Extra score: wall-node swing (how much the battery is charged/discharged).
Run: python scripts/sat_curve_selftune_study3.py --weather <json> [--out file]
"""
import itertools, json, math, os, random, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study2 as s2
from sat_curve_selftune_study import curve, TARGET, DT

ERAS = {  # era: (H total W/K, G_wo opaque W/K)  -- estimates, see the research page
    "1_pre1965": (620, 420), "2_1965-75": (440, 280), "3_1983-92": (250, 115),
    "4_1992-2012": (160, 66), "5_2015": (95, 40), "6_BENG2021": (80, 39)}
G_AW = 2880.0

def house(era, mass, kr):
    Htot, Gwo = ERAS[era]; Gao = Htot - Gwo
    C = (260e3 if mass == "heavy" else 110e3) * 120.0
    Ca, Cw = 0.1 * C, 0.9 * C
    Heff = Gao + 1.0 / (1.0 / G_AW + 1.0 / Gwo)
    return Ca, Cw, Gao, Gwo, Heff, kr * Heff

def best_coeff2(heff, K, to):
    if to >= TARGET - 0.5: return None
    need = (heff * (TARGET - to) / K) ** (1 / 1.3) + 5 + TARGET
    return min((abs(curve(c / 10, to) - need), c / 10) for c in range(1, 51))[1]

def run(args):
    spec, era, mass, kr, c_off, seed = args[:6]
    trace = [] if len(args) > 6 and args[6] else None
    tm, sm = base.WEATHER["w"]; minutes = len(tm)
    Ca, Cw, Gao, Gwo, Heff, K = house(era, mass, kr)
    rnd = random.Random(seed); gains = [0.0] * minutes
    for d in range(minutes // 1440):
        for m in range(d * 1440 + 1050, d * 1440 + 1110): gains[m] += 1200.0
        for _ in range(rnd.randint(1, 3)):
            st = d * 1440 + int(rnd.uniform(8, 22) * 60); w = rnd.uniform(300, 900)
            for m in range(st, min(minutes, st + int(rnd.uniform(30, 120)))): gains[m] += w
        if d % 14 == 9:
            for m in range(d * 1440 + 1140, d * 1440 + 1440): gains[m] += 1500.0
    season = statistics.fmean(t for t in tm if t < 15)
    cbest = best_coeff2(Heff, K, season)
    c = round(min(5.0, max(0.1, cbest + c_off)), 3)
    st = s2.make(spec)
    Ta = TARGET; Tw = TARGET - (TARGET - season) * Gwo / (Gwo + G_AW) * 0.0 + 0.0
    Tw = (G_AW * TARGET + Gwo * season) / (G_AW + Gwo)        # start the battery charged for the mean weather
    flow = 30.0; integ = 0.0; last = None; rd = 0.0
    corr_acc = 0.0; corr_n = 0; clean = True; stepped = False; tout_acc = 0.0
    sq = 0.0; n = 0; out_band = 0; over = 0; heat = 0.0; tw_min = 99.0; tw_max = -99.0
    writes = flips = wrong = 0; last_dir = 0; conv_day = None; maxdc = 0.0
    for m in range(minutes):
        day = m // 1440; tout, sw = tm[m], sm[m]; heating = tout < 15
        cv = curve(c, tout); kp = c * cv / 3; ki = kp / 8400; kd = 588.0 * kp
        err = TARGET - Ta
        if last is not None:
            d_ = Ta - last; rd = 0.0 if d_ == 0 else -d_ / DT
        last = Ta
        if abs(err) > 0.1: integ = 0.0
        else: integ = max(-cv, min(cv, integ + ki * err * DT))
        sp = round(cv + kp * err + integ + kd * rd, 1) if heating else 10.0
        draw = (420 <= m % 1440 < 430) or (1140 <= m % 1440 < 1150)
        flow += ((min(sp, 70.0) if not draw else Ta) - flow) * DT / 300.0
        q = K * max(flow - 5.0 - Ta, 0.0) ** 1.3 if not draw else 0.0
        solar = 1.5 * sw
        qaw = G_AW * (Ta - Tw)
        Ta += (q + gains[m] + 0.5 * solar - qaw - Gao * (Ta - tout)) * DT / Ca
        Tw += (qaw + 0.5 * solar - Gwo * (Tw - tout)) * DT / Cw
        if heating:
            corr_acc += sp - cv; corr_n += 1; tout_acc += tout
            if day >= 14:
                sq += err * err; n += 1; heat += q * DT
                if abs(Ta - TARGET) > 0.3: out_band += 1
                if Ta > TARGET + 0.3: over += 1
                tw_min = min(tw_min, Tw); tw_max = max(tw_max, Tw)
        clean &= heating and not draw and sw < 50.0
        if trace is not None and m % 60 == 0:
            trace.append((m // 60, round(tout, 2), round(sw, 1), round(Ta, 3), round(Tw, 3), sp, cv, c, gains[m]))
        if m % 60 == 59:
            mc = corr_acc / corr_n if corr_n else 0.0; mt = tout_acc / corr_n if corr_n else tout
            new_c = st.hour(c, day, mc, clean and not stepped and corr_n > 0, mt)
            corr_acc = 0.0; corr_n = 0; tout_acc = 0.0; clean = True; stepped = False
            if new_c != c:
                b = best_coeff2(Heff, K, statistics.fmean(tm[max(0, m - 1439):m + 1]))
                if b is not None and abs(new_c - b) > abs(c - b) + 1e-9: wrong += 1
                dr = 1 if new_c > c else -1
                if last_dir and dr != last_dir: flips += 1
                maxdc = max(maxdc, abs(new_c - c)); last_dir = dr; writes += 1; c = new_c; integ = 0.0; rd = 0.0; stepped = True
        if m % 1440 == 1439 and conv_day is None and abs(c - cbest) <= 0.1: conv_day = day
    if trace is not None:
        return {"spec": st.name, "trace": trace}
    days = max(1.0, n / 1440)
    return {"spec": st.name, "era": era, "mass": mass, "kr": kr, "c_off": c_off, "seed": seed,
            "tau_wall_h": round(Cw / (Gwo + 1.0 / (1.0 / G_AW + 1.0 / max(Gao, 1.0)) ) / 3600, 1),
            "rms": math.sqrt(sq / n), "out_band_min_d": out_band / days, "over_min_d": over / days, "heat_J": heat,
            "wall_swing": round(tw_max - tw_min, 2), "writes": writes, "flips": flips, "wrong": wrong,
            "conv_day": conv_day, "c_end": c, "cbest": cbest, "maxdc": maxdc}

if __name__ == "__main__":
    wpath = sys.argv[sys.argv.index("--weather") + 1]
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else "study3_results.json"
    specs = [("none",)]
    for W, N, THR, GAIN in itertools.product((6,), (3,), (1.0,), (0.6,)):                  # the fast reference
        specs.append(("P", W, N, THR, GAIN, 12))
    for W, GAP, THR, GAIN in itertools.product((24, 72), (60, 72), (1.0, 2.0), (0.3, 0.6)):   # entry 168 waits
        specs.append(("P", W, 1, THR, GAIN, GAP))
    for W, GAP, GAIN in itertools.product((24, 72), (72,), (0.3, 0.6)):                      # + comfort fast lane
        specs.append(("P", W, 1, 1.0, GAIN, GAP, 4, 4.0))
    houses = [(e, mass, kr) for e in ERAS for mass in ("heavy", "light") for kr in (0.8, 1.0, 1.2)]
    offsets = (-1.0, -0.5, 0.0, 0.5, 1.0)
    jobs = [(s, *h, off, 1) for s in specs for h in houses for off in offsets]
    print(f"{len(specs)} strategies x {len(houses)} houses x {len(offsets)} start curves = {len(jobs)} runs", flush=True)
    with Pool(6, initializer=base._init, initargs=(wpath,)) as p:
        res = p.map(run, jobs, chunksize=4)
    ref = {(r["era"], r["mass"], r["kr"]): r["heat_J"] for r in res if r["spec"] == "none" and r["c_off"] == 0.0}
    for r in res: r["gas_pct"] = 100.0 * r["heat_J"] / ref[(r["era"], r["mass"], r["kr"])]
    json.dump(res, open(out, "w"))
    print("done", len(res))
