"""TASK-1563 round 24 (2026-11-02): the key SAT comparisons over the Dutch housing stock (RVO Voorbeeldwoningen 2022,
51 dwellings, weighted by their number of dwellings), ten heating seasons 2015-16 .. 2024-25.

Houses: tools/research/sat/sat_stock_houses.py (RVO areas and U-values; assumptions A1-A6 in its docstring).
Groups (the variants within a group run on the same dwelling, weather and seed, so every comparison is paired):
  gas   state 'huidig':  gas_boiler_min35 and gas_boiler_min55, variants 'cont' (SAT never enters PWM) and 'sat'
                         (SAT's overshoot switch to gas PWM).
  hp    state 'pakket2' (RVO's heat-pump package): the smallest electric unit of the round 21 library whose maximum
                         at -7 C covers the design load at -10 C (the largest if none does), variants 'cont' and 'sat',
                         flow cap 40 C (entry 155) and 55 C (RVO's 55/47 radiators in pakket 2).
  hybrid state 'huidig': remeha_elga_ace_4 and intergas_xtend_5 with the separator, boiler 'own' vs 'slave' (round 22),
                         master SAT continuous.
Curve per dwelling: the balance coefficient (the curve that delivers the season-mean heat at the season-mean outdoor
temperature through this dwelling's emitter); the same for every variant of a dwelling (an installer's setting).
Run: python tools/research/sat/sat_stock_study24.py --seasondir docs/sat/sim-hp-lowload/weather_season10 --outdir <dir>
       --group gas|hp|hybrid [--workers N] [--limit N]
"""
import glob, json, os, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_hp_library_study21 as lib
import sat_stock_houses as SH
import sat_sim_port as port
import sat_curve_selftune_study as base

ELECTRIC = [k for k, u in lib.UNITS.items() if u["type"] == "electric"]


def pmax_at(u, t):
    return lib.interp([(tt, v * 1000.0) for tt, v in u["pmax"]], t)


def pick_unit(design_w):
    fits = sorted((pmax_at(lib.UNITS[k], -7), k) for k in ELECTRIC if pmax_at(lib.UNITS[k], -7) >= design_w)
    return fits[0][1] if fits else max((pmax_at(lib.UNITS[k], -7), k) for k in ELECTRIC)[1]


def balance_coeff(h, season, base_off, mcp):
    q = max(h["Heff"] * (lib.TARGET - season) - 135.0 * h["gain_scale"], 50.0)
    flow = lib.TARGET + (q / h["K"]) ** (1.0 / h["exp"]) + q / (2 * mcp)
    best = min((abs(float(port.curve(c / 100.0, lib.TARGET, season, base_off)) - flow), c / 100.0) for c in range(10, 501))
    return best[1]


def jobs(group, limit=None):
    d = SH.load(); J = []
    for s in d["stock"][:limit]:
        if group == "gas":
            h = SH.house(s["type"], s["period"], "huidig", d)
            for un in ("gas_boiler_min35", "gas_boiler_min55"):
                for v in ("cont", "sat"): J.append((group, h, un, v, 62.0))
        elif group == "hp":
            h = SH.house(s["type"], s["period"], "pakket2", d); un = pick_unit(h["design_load_w"])
            for cap in (40.0, 55.0):
                for v in ("cont", "sat"): J.append((group, h, un, v, cap))
        else:
            h = SH.house(s["type"], s["period"], "huidig", d)
            for un in ("remeha_elga_ace_4", "intergas_xtend_5"):
                for mode in ("own", "slave"): J.append((group, h, un, mode, 62.0))
    return J


def _init(w): lib._init(w)


def job(a):
    group, h, un, v, cap = a
    lib.HOUSE = h; lib.PLANT = h["emitter"]; lib.FLOW_CAP = cap
    lib.OV_MARGIN = 2.0; lib.OV_TICKS = 1; lib.MM_PWM = 100.0
    lib.PWM_PERIOD_S, lib.PWM_MINON_S, lib.PWM_MINOFF_S = 1800, 600, 600
    tm, _ = base.WEATHER["w"]; season = statistics.fmean(t for t in tm if t < 15)
    ufh = h["emitter"] == "ufh"; base_off = 20.0 if ufh else 27.2
    c = balance_coeff(h, season, base_off, lib.h12.MCP_UFH * h["gain_scale"])
    screed = (8.0e6 if ufh else 0.6e6) * h["gain_scale"]                  # (A) scaled by floor area
    if group == "hybrid":
        lib.HYBRID_MODE = v; lib.SEPARATOR = True; variant = "cont"
    else:
        lib.HYBRID_MODE = "native"; lib.SEPARATOR = False; variant = v
    r = lib.run((un, variant, c, h["id"], screed))
    r.update(group=group, dwelling=h["id"], mode=v, flow_cap=cap, dwellings=h["dwellings"], design_load_w=h["design_load_w"],
             emitter=h["emitter"])
    return r


if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    group = arg("--group"); workers = int(arg("--workers")) if "--workers" in sys.argv else 4
    limit = int(arg("--limit")) if "--limit" in sys.argv else None
    J = jobs(group, limit)
    part = os.path.join(arg("--outdir"), f"round24_{group}_parts"); os.makedirs(part, exist_ok=True); res = {}
    for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))):
        label = os.path.basename(w)[7:-5]; dest = os.path.join(part, label + ".json")
        if os.path.exists(dest): res[label] = json.load(open(dest)); continue
        with Pool(workers, initializer=_init, initargs=(w,)) as p: res[label] = p.map(job, J, chunksize=1)
        json.dump(res[label], open(dest, "w")); print(group, label, "done", flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), f"round24_{group}_runs.json"), "w")); print("done")
