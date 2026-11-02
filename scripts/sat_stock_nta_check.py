"""TASK-1563: plausibility check of the stock houses (sat_stock_houses.py) against the NTA 8800 heat demand that RVO
publishes per dwelling and level. Steady state per hour over the ten heating seasons (Oct-Apr) of weather_season10:
max(0, Heff x (20 - T) - gains - solar). Not a calibration of single dwellings; it checks the whole set.
Run: python scripts/sat_stock_nta_check.py
"""
import glob, json, os, statistics as st, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_stock_houses as S

W = [json.load(open(f))["hourly"] for f in sorted(glob.glob(os.path.join(os.path.dirname(S.DATA), "..",
     "sim-hp-lowload", "weather_season10", "season_*.json")))]


def demand(h):
    out = []
    for w in W:
        q = sum(max(0.0, h["Heff"] * (20 - T) - 135.0 * h["gain_scale"] - 1.5 * (sw or 0) * h["solar_scale"])
                for T, sw in zip(w["temperature_2m"], w["shortwave_radiation"]) if T is not None)
        out.append(q / 1000 / h["Ag"])
    return st.fmean(out)


if __name__ == "__main__":
    d = S.load(); res = {}
    for lv in ("oorspronkelijk", "huidig", "pakket2", "pakket3"):
        r = [(h["id"], demand(h), h["heat_demand_nta_kwh_m2"], h["dwellings"]) for h in S.all_dwellings(lv, d)]
        rat = sorted(a[1] / a[2] for a in r if a[2]); ws = sum(a[3] for a in r)
        res[lv] = {"median_ratio": st.median(rat), "p10": rat[len(rat) // 10], "p90": rat[9 * len(rat) // 10],
                   "model_kwh_m2_weighted": sum(a[1] * a[3] for a in r) / ws, "nta_kwh_m2_weighted": sum(a[2] * a[3] for a in r) / ws,
                   "per_dwelling": [(a[0], round(a[1], 1), round(a[2], 1)) for a in r]}
        print(lv, "model/NTA median %.2f p10 %.2f p90 %.2f | stock-weighted model %.0f NTA %.0f kWh/m2" % (
            res[lv]["median_ratio"], res[lv]["p10"], res[lv]["p90"], res[lv]["model_kwh_m2_weighted"], res[lv]["nta_kwh_m2_weighted"]))
    json.dump(res, open(os.path.join(os.path.dirname(S.DATA), "nta_check.json"), "w"), indent=1)
