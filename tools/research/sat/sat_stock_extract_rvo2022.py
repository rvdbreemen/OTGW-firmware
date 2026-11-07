"""TASK-1563: extract the Dutch housing-stock reference dwellings from RVO 'Voorbeeldwoningen 2022 bestaande bouw'.

Primary sources (RVO, 2023-01, https://www.rvo.nl/onderwerpen/wetten-en-regels-gebouwen/voorbeeldwoningen-bestaande-bouw):
  - data-voorbeeldwoningen-2022.xlsx: per dwelling type x period, sheet '(gem) <type> <period>' (the 'gemiddeld'
    variant), five energy levels in columns: oorspronkelijk, huidig (WoON2018), pakket 1, 2, 3.
  - brochure-voorbeeldwoningen-bestaande-bouw-2022.pdf (text extracted to .txt): number of dwellings and stock share
    per reference dwelling ("vertegenwoordigen met N woningen, X% van de Nederlandse woningvoorraad").
Downloaded files are data only; run with python -I and pass paths as arguments.
Run: python -I tools/research/sat/sat_stock_extract_rvo2022.py <data.xlsx> <brochure.txt> <out.json>
"""
import json, re, sys
import openpyxl

LEVELS = ("oorspronkelijk", "huidig", "pakket1", "pakket2", "pakket3")
ELEMENTS = {"Begane grondvloer": "floor", "Dichte gevel": "facade", "Hellend / plat dak": "roof", "Ramen": "window",
            "Deuren": "door"}
TEXT_ROWS = {"ventilatietype": "ventilation", "type afgiftesysteem": "emitter", "temperatuurniveau": "emitter_temp",
             "toestel ruimteverwarming": "heater", "kierdichting (q;v10)": "airtightness"}


def num(x):
    try: return float(x)
    except (TypeError, ValueError): return None


def read_sheet(ws):
    rows = [list(r) for r in ws.iter_rows(values_only=True)]
    d = {"levels": {lv: {} for lv in LEVELS}}
    for r in rows:
        cells = [v for v in r if v not in (None, "")]                 # the label is the first filled cell
        if not cells: continue
        key, vals = str(cells[0]).strip(), cells[1:]
        if key == "voorbeeldwoning" and vals: d["type"] = str(vals[0])
        elif key == "bouwperiode" and vals: d["period"] = str(vals[0])
        elif key.startswith("gebruiksoppervlakte") and vals and "Ag_m2" not in d: d["Ag_m2"] = num(vals[0])
        elif key.startswith("begrenzing beganegrond") and vals and "ground_floor_over" not in d: d["ground_floor_over"] = str(vals[0])
        elif key.startswith("aantal bouwlagen") and vals and "storeys" not in d: d["storeys"] = num(vals[0])
        elif key.startswith("gebouwtype") and vals and "building_type" not in d: d["building_type"] = str(vals[0])
        elif key in ELEMENTS and "u" not in d["levels"]["huidig"].get(ELEMENTS[key], {}):
            el = ELEMENTS[key]
            step = 2 if el in ("window", "door") else 3            # (area, U) or (area, Rc, U)
            for i, lv in enumerate(LEVELS):
                chunk = vals[i * step:(i + 1) * step]
                if len(chunk) == step:
                    d["levels"][lv][el] = {"area_m2": num(chunk[0]), "u_w_m2k": num(chunk[-1])}
        elif key in TEXT_ROWS:
            for i, lv in enumerate(LEVELS):
                if i < len(vals): d["levels"][lv][TEXT_ROWS[key]] = str(vals[i])
        elif key.startswith("Warmtebehoefte verwarm") or key.startswith("warmtebehoefte woning"):
            for i, lv in enumerate(LEVELS):
                if i < len(vals) and num(vals[i]) is not None: d["levels"][lv]["heat_demand_kwh_m2"] = num(vals[i])
    for lv in LEVELS:                                             # transmission H (W/K) = sum U x A, no corrections
        L = d["levels"][lv]
        parts = [L[e]["area_m2"] * L[e]["u_w_m2k"] for e in ELEMENTS.values()
                 if e in L and L[e]["area_m2"] and L[e]["u_w_m2k"]]
        L["H_trans_simple_w_k"] = round(sum(parts), 1) if parts else None
    return d


TYPE_KEYS = (("vrijstaande", "vrij"), ("1 kap", "2-1 kap"), ("rijwoningen (tussen)", "rij tuss"),
             ("rijwoningen (hoek)", "rij hoek"), ("maisonnette", "mais"), ("galerij", "gal"), ("portiek", "port"),
             ("flatwoningen", "overig"))
PERIODS = {(None, 1945): "tot46", (None, 1964): "tot65", (1946, 1964): "46-64", (1965, 1974): "65-74",
           (1975, 1991): "75-91", (1992, 2005): "92-05", (2006, 2014): "06-14", (2015, 2018): "15-18"}


def stock_counts(txt):
    """Every sentence '<type> ... gebouwd <period> ... vertegenwoordigen met N woningen ... X%' in the brochure body
    (chapters 3-10). Type and period are read from the sentence itself."""
    t = re.sub(r"\s+", " ", re.sub(r"-\s*\n\s*", "", txt.replace("­", "")))
    out = []
    for c in re.finditer(r"met ([\d]{1,3}(?:\.\d{3})*) woningen\W{0,2}(?:[^\d%]{0,22})([\d,]+)\s*%", t):
        pre = t[max(0, c.start() - 220):c.start()]
        s = pre[pre.rfind(". ") + 2:] if ". " in pre else pre                 # the sentence that holds the count
        typ = next((code for key, code in TYPE_KEYS if key in s.lower()), None)
        if typ is None and "rijwoningen" in s.lower():
            typ = "rij hoek" if "(hoek)" in s else "rij tuss"
        yrs = [int(y) for y in re.findall(r"(19\d{2}|20\d{2})", s) if int(y) <= 2018]   # 2022 = the brochure's title
        if "vooroorlogse" in s.lower(): per = "tot46"
        elif "tot en met" in s and len(set(yrs)) == 1: per = PERIODS.get((None, yrs[0]))
        elif len(yrs) >= 2: per = PERIODS.get((yrs[-2], yrs[-1]))
        else: per = None
        out.append({"type": typ, "period": per, "dwellings": int(c.group(1).replace(".", "")),
                    "share_pct": float(c.group(2).replace(",", ".")), "sentence": s.strip()[:200]})
    return out


if __name__ == "__main__":
    xlsx, txt, out = sys.argv[1:4]
    wb = openpyxl.load_workbook(xlsx, read_only=True, data_only=True)
    sheets = {ws.title: read_sheet(ws) for ws in wb.worksheets if ws.title.startswith("(gem)")}
    res = {"source": "RVO Voorbeeldwoningen 2022 bestaande bouw (data-voorbeeldwoningen-2022.xlsx, brochure 2023-01)",
           "sheets": sheets, "stock": stock_counts(open(txt, encoding="utf-8").read())}
    json.dump(res, open(out, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    print(len(sheets), "sheets,", len(res["stock"]), "stock entries")
