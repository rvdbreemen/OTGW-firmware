"""TASK-1563: turn the RVO Voorbeeldwoningen 2022 dwellings into parameters for the SAT two-node house model.

Facts come from docs/sat/sim-stock/rvo_voorbeeldwoningen_2022.json (RVO data sheet + brochure). Every value that is
not in that source is an assumption, marked (A) here and listed in docs/sat/sim-stock/README.md:

  (A1) floor to a crawl space / unheated space / ground loses with a reduction factor B_FLOOR = 0.5; a floor above
       outside air gets 1.0; apartment floors and roofs to heated neighbours are not in the RVO areas (no loss).
  (A2) ventilation heat loss 0.34 W/(m3 K) x n x V with V = Ag x 2.6 m and n by RVO ventilation type: fully natural
       0.7 /h, mechanical exhaust 0.5 /h, balanced with heat recovery 0.15 /h (effective).
  (A2b) with the RVO sealing of the packages (qv10 0.7 or 0.4 dm3/s.m2): mechanical exhaust 0.35 /h, balanced 0.12 /h.
       Chosen after the plausibility check against the NTA 8800 heat demand (pakket 2 came out 23 % high with 0.5).
  (A3) thermal capacity per m2 floor area (ISO 13790 classes, research page): heavy 260 kJ/m2K for masonry and
       concrete dwellings, medium 165 for detached/semi-detached/terraced built up to 1964 (timber ground floors and
       roofs, brochure p.25: "vaak een begane grondvloer die van hout is").
  (A4) air node 10 % of C; air-to-wall coupling 24 W/K per m2 floor (360 m2 interior x 8 W/m2K for 120 m2, scaled).
  (A5) internal gains and solar aperture scaled by floor area (gains) and window area (solar) against the 120 m2 /
       25 m2 reference house of rounds 3-23.
  (A6) emitters sized at the design point -10 C outside / 20 C inside: radiators for the *oorspronkelijk* heat loss at
       mean water 80 C (90/70, the as-built design), exponent 1.3; floor heating where RVO lists it, for the heat loss
       of the state that has it at mean water 42.5 C (45/40), exponent 1.1.
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "..", "..", "docs", "sat", "sim-stock", "rvo_voorbeeldwoningen_2022.json")
B_FLOOR = 0.5
N_VENT = {"volledig natuurlijk": 0.7, "natuurlijke toevoer, mechanische afvoer": 0.5, "volledig mechanisch": 0.15}
N_SEALED = {"natuurlijke toevoer, mechanische afvoer": 0.35, "volledig mechanisch": 0.12}   # (A2b) with sealing
C_HEAVY, C_MEDIUM = 260e3, 165e3
T_DESIGN_OUT, T_IN = -10.0, 20.0
OPAQUE = ("floor", "facade", "roof")


def load():
    return json.load(open(DATA, encoding="utf-8"))


def sheet_name(typ, period):
    s = f"(gem) {typ} {period}"
    return s if typ in ("vrij", "2-1 kap", "rij tuss", "rij hoek") else s + " GEM"


def losses(w, level):
    """(G_opaque, G_glazing+doors, H_vent) in W/K for one energy level."""
    L = w["levels"][level]; Ag = w["Ag_m2"] or 0.0
    fb = 1.0 if w.get("ground_floor_over") == "buiten" else B_FLOOR
    ua = lambda e, f=1.0: (L[e]["area_m2"] or 0) * (L[e]["u_w_m2k"] or 0) * f if e in L else 0.0
    g_op = ua("floor", fb) + ua("facade") + ua("roof")
    g_gl = ua("window") + ua("door")
    n = N_VENT.get(L.get("ventilation"), 0.5)
    if str(L.get("airtightness", "forfaitair")).startswith("0,"):   # sealed (RVO packages: qv10 0.7 or 0.4)
        n = N_SEALED.get(L.get("ventilation"), n)
    h_v = 0.34 * n * Ag * 2.6
    return g_op, g_gl, h_v


def house(typ, period, level, data=None):
    d = data or load()
    w = d["sheets"][sheet_name(typ, period)]
    Ag = w["Ag_m2"]
    old_frame = typ in ("vrij", "2-1 kap", "rij tuss", "rij hoek") and period in ("tot46", "46-64", "tot65")
    C = (C_MEDIUM if old_frame else C_HEAVY) * Ag
    Ca, Cw = 0.1 * C, 0.9 * C
    g_aw = 24.0 * Ag
    g_op, g_gl, h_v = losses(w, level)
    Gwo, Gao = g_op, g_gl + h_v
    Heff = Gao + 1.0 / (1.0 / g_aw + 1.0 / max(Gwo, 1e-6))
    L = w["levels"][level]
    ufh = L.get("emitter", "").startswith("vloer")
    if ufh:   # floor heating sized for this state at 45/40
        q_d = Heff * (T_IN - T_DESIGN_OUT); K = q_d / (42.5 - T_IN) ** 1.1; exp_e = 1.1
    else:     # radiators sized for the as-built heat loss at 90/70 (A6)
        go, gg, hv = losses(w, "oorspronkelijk")
        heff0 = gg + hv + 1.0 / (1.0 / g_aw + 1.0 / max(go, 1e-6))
        q_d = heff0 * (T_IN - T_DESIGN_OUT); K = q_d / (80.0 - T_IN) ** 1.3; exp_e = 1.3
    win = (L.get("window", {}).get("area_m2") or 0) + 0.0
    stock = next((s for s in d["stock"] if s["type"] == typ and s["period"] == period), None)
    return {"id": f"{typ}|{period}|{level}", "Ag": Ag, "Ca": Ca, "Cw": Cw, "Gao": Gao, "Gwo": Gwo, "G_aw": g_aw,
            "Heff": Heff, "design_load_w": Heff * (T_IN - T_DESIGN_OUT), "emitter": "ufh" if ufh else "rad",
            "K": K, "exp": exp_e, "gain_scale": Ag / 120.0, "solar_scale": win / 25.0 if win else Ag / 120.0,
            "heat_demand_nta_kwh_m2": L.get("heat_demand_kwh_m2"),
            "dwellings": stock["dwellings"] if stock else None, "share_pct": stock["share_pct"] if stock else None}


def all_dwellings(level, data=None):
    d = data or load()
    return [house(s["type"], s["period"], level, d) for s in d["stock"]]


if __name__ == "__main__":
    d = load()
    for lv in ("oorspronkelijk", "huidig", "pakket2"):
        hs = all_dwellings(lv, d)
        print(lv, "design load kW (stock-weighted mean)",
              round(sum(h["design_load_w"] * h["dwellings"] for h in hs) / sum(h["dwellings"] for h in hs) / 1000, 2))
    for h in all_dwellings("huidig", d)[:6]:
        print(h["id"], round(h["Heff"]), "W/K", round(h["design_load_w"] / 1000, 1), "kW", h["emitter"])
