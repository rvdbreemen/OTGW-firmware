"""TASK-1563 dashboard: one heating season minute by minute, to show what SAT does and how well it controls.

Case: NIBE S2125-8 (round 21 library), 2015 house on a heavy underfloor screed, winter 2024-25 (1 Oct .. 30 Apr,
Open-Meteo, 52.13 N 6.13 E), curve 1.6 (round 21 best for both variants). Two variants on the same house, weather and
gains: 'cont' (SAT continuous, the heat pump modulates and cycles by itself) and 'sat' (SAT with its overshoot switch
to PWM, period 30 min, minimum ON/OFF 10 min). SAT through the proven port (tools/research/sat/sat_sim_port.py).
Outputs (json, rows):
  <out>/winter_hourly.json  per hour and variant: outdoor, room, CS, flow, heat-pump kW, share of the hour on, starts
  <out>/winter_detail.json  per 5 minutes for four weeks (coldest, mildest, typical and last week)
  <out>/winter_quality.json per variant: control-quality figures and a histogram of the room error
  <out>/winter_weekly.json  per ISO week and variant: room error, share within +-0.3 C, starts per day
Run: python tools/research/sat/sat_winter_trace.py <weather season json> <out dir>
"""
import datetime as dt, json, os, statistics as st, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_hp_library_study21 as lib

UNIT, ERA, SCREED, COEFF = "nibe_s2125_8", "5_2015", 8.0e6, 1.6
VARIANTS = {"cont": "SAT continuous", "sat": "SAT with PWM"}
WARMUP_MIN = 14 * 1440   # the first two weeks settle the house; quality is scored after them (as in run())


def trace(variant):
    lib.TRACE = []
    lib.HOUSE = None; lib.PLANT = "ufh"; lib.SEPARATOR = False; lib.HYBRID_MODE = "native"
    lib.PWM_PERIOD_S, lib.PWM_MINON_S, lib.PWM_MINOFF_S = 1800, 600, 600
    r = lib.run((UNIT, variant, COEFF, ERA, SCREED))
    t = lib.TRACE; lib.TRACE = None
    return r, t


if __name__ == "__main__":
    wpath, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    lib._init(wpath)
    start = dt.datetime.fromisoformat(json.load(open(wpath))["hourly"]["time"][0])
    hourly, detail, quality, weekly = [], [], [], []
    traces = {}
    for v, name in VARIANTS.items():
        r, tr = trace(v); traces[v] = tr
        # hourly
        for h in range(len(tr) // 60):
            blk = tr[h * 60:(h + 1) * 60]
            cs = [x[2] for x in blk if x[2] is not None]
            starts = sum(1 for i in range(1, len(blk)) if blk[i][5] and not blk[i - 1][5]) + (1 if blk[0][5] and h and not tr[h * 60 - 1][5] else 0)
            hourly.append({"t": (start + dt.timedelta(hours=h)).strftime("%Y-%m-%dT%H:00"), "variant": name,
                           "outdoor_c": round(st.fmean(x[0] for x in blk), 2), "room_c": round(st.fmean(x[1] for x in blk), 3),
                           "cs_c": round(st.fmean(cs), 2) if cs else None, "flow_c": round(st.fmean(x[3] for x in blk), 2),
                           "hp_kw": round(st.fmean(x[4] for x in blk) / 1000, 3), "on_share": round(st.fmean(x[5] for x in blk), 3),
                           "starts": starts})
        # quality, after the warm-up, heating minutes only (CS present)
        err = [x[1] - lib.TARGET for i, x in enumerate(tr) if i >= WARMUP_MIN and x[2] is not None]
        bins = [round(-1.0 + 0.1 * k, 1) for k in range(21)]
        hist = {b: 0 for b in bins}
        for e in err:
            b = round(max(-1.0, min(1.0, round(e * 10) / 10)), 1); hist[b] += 1
        days = len(tr) / 1440
        runs, cur = [], 0
        for x in tr:
            if x[5]: cur += 1
            elif cur: runs.append(cur); cur = 0
        quality.append({"variant": name, "mean_abs_err_c": round(st.fmean(abs(e) for e in err), 3),
                        "within_03_pct": round(100 * sum(abs(e) <= 0.3 for e in err) / len(err), 1),
                        "within_05_pct": round(100 * sum(abs(e) <= 0.5 for e in err) / len(err), 1),
                        "coldest_c": round(min(err) + lib.TARGET, 2), "warmest_c": round(max(err) + lib.TARGET, 2),
                        "starts_per_day": round(r["starts_per_day"], 2), "median_run_min": round(st.median(runs), 1) if runs else 0,
                        "pwm_share_pct": round(100 * r["pwm_share"], 1), "elec_kwh": round(r["elec_kwh"]),
                        "hist": [{"err_c": b, "share_pct": round(100 * hist[b] / len(err), 2)} for b in bins]})
        # weekly
        wk = {}
        for i, x in enumerate(tr):
            if i < WARMUP_MIN or x[2] is None: continue
            d = start + dt.timedelta(minutes=i); key = d.strftime("%G-W%V")
            wk.setdefault(key, []).append((x[1] - lib.TARGET, x[5]))
        for key, L in wk.items():
            on = [o for _, o in L]
            st_ = sum(1 for i in range(1, len(on)) if on[i] and not on[i - 1])
            weekly.append({"week": key, "variant": name, "mean_abs_err_c": round(st.fmean(abs(e) for e, _ in L), 3),
                           "within_03_pct": round(100 * sum(abs(e) <= 0.3 for e, _ in L) / len(L), 1),
                           "starts_per_day": round(st_ / (len(L) / 1440), 2)})
    # four weeks at 5-minute resolution: coldest, mildest, typical (closest to the season mean) and last
    days = len(traces["cont"]) // 1440
    day_mean = [st.fmean(x[0] for x in traces["cont"][d * 1440:(d + 1) * 1440]) for d in range(days)]
    wk_mean = {d: st.fmean(day_mean[d:d + 7]) for d in range(14, days - 7)}
    picks = {"Coldest week": min(wk_mean, key=wk_mean.get), "Mildest week": max(wk_mean, key=wk_mean.get),
             "Typical week": min(wk_mean, key=lambda d: abs(wk_mean[d] - st.fmean(day_mean))), "Last week": days - 8}
    for label, d0 in picks.items():
        for v, name in VARIANTS.items():
            tr = traces[v]
            for i in range(d0 * 1440, (d0 + 7) * 1440, 5):
                blk = tr[i:i + 5]; cs = [x[2] for x in blk if x[2] is not None]
                detail.append({"week": label, "t": (start + dt.timedelta(minutes=i)).strftime("%Y-%m-%dT%H:%M"), "variant": name,
                               "outdoor_c": round(blk[0][0], 1), "room_c": round(st.fmean(x[1] for x in blk), 3),
                               "cs_c": round(st.fmean(cs), 1) if cs else None, "flow_c": round(st.fmean(x[3] for x in blk), 1),
                               "hp_kw": round(st.fmean(x[4] for x in blk) / 1000, 2), "on": max(x[5] for x in blk)})
    for name, rows in (("winter_hourly", hourly), ("winter_detail", detail), ("winter_quality", quality), ("winter_weekly", weekly)):
        json.dump(rows, open(os.path.join(out, name + ".json"), "w", encoding="utf-8"), separators=(",", ":"))
        print(name, len(rows), "rows", os.path.getsize(os.path.join(out, name + ".json")), "bytes")
    for q in quality: print({k: v for k, v in q.items() if k != "hist"})
    print("weeks:", {k: (start + dt.timedelta(days=d)).strftime("%Y-%m-%d") for k, d in picks.items()})
