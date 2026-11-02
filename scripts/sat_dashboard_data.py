"""Summarise every SAT simulation round into one table for the results dashboard (TASK-1563).

One row per (round, setting): the mean over all runs of that setting (houses, seasons, seeds) of the metrics the round
recorded. Metrics that a round did not record stay null. Labels are plain English; the raw codes are kept in `code`.
Sources: docs/sat/sim-1519/data/ (curve self-tune, TASK-1519, rounds 1-11) and docs/sat/sim-hp-lowload/ (heat pumps,
TASK-1527, rounds 12-23). Run from the repo root: python scripts/sat_dashboard_data.py <out.json>
"""
import csv, glob, json, os, statistics as st, sys
from collections import defaultdict

T = "docs/sat/sim-1519/data/"; H = "docs/sat/sim-hp-lowload/"
SRC = {"hp30": "modulating 30-100 %", "hp60": "modulating 60-100 %", "hp100": "on/off heat pump", "gas": "gas boiler"}
UNIT = {"remeha_elga_ace_4": "Remeha Elga Ace 4", "atag_energion_m_hybrid_5": "ATAG Energion M 5", "intergas_xtend_5": "Intergas Xtend 5",
        "bosch_compress_7000i_aw_5": "Bosch Compress 7000i 5", "mitsubishi_ecodan_puz_wm50": "Mitsubishi Ecodan WM50",
        "nibe_s2125_8": "NIBE S2125-8", "daikin_altherma3_r_erga04": "Daikin Altherma 3 R 04", "vaillant_arotherm_plus_7": "Vaillant aroTHERM plus 7",
        "daikin_altherma3_r_erga08": "Daikin Altherma 3 R 08", "panasonic_aquarea_j_7": "Panasonic Aquarea J 7"}
CAND = {"current": "Current (30 min, ON/OFF 10 min)", "period3600": "Period 60 min", "period10800": "Period 3 h",
        "old_minon": "Old rule (ON 3 min, no min OFF)", "cap35": "Flow cap 35 C", "cap45": "Flow cap 45 C", "mm0": "MM 0 in PWM",
        "period2400": "Period 40 min", "ov_fw60": "Switch timing as firmware", "ov_margin3": "Switch margin 3 C (model artefact)",
        "ov_margin4": "Switch margin 4 C (model artefact)", "ov_sustain300": "Switch after 300 s",
        "cont_hyst_3_1": "Unit band 3 K / 1 K", "cont_hyst_1_1": "Unit band 1 K / 1 K", "cont_hyst_5_3": "Unit band 5 K / 3 K"}


def gain(k):
    k = str(k)
    return "SAT PID" if k == "sat" else "curve only, no room feedback" if k == "wc" else f"room factor {k}"


def setback(sb):
    if sb in (None, 0, "0", False): return ""
    q = list(sb); return f" · setback {q[0]:g} K {int(q[1]):02d}:00-{int(q[2]):02d}:00"


PLANT = {"onoff_ufh": "on/off, underfloor", "mod_rad": "modulating, radiators", "ufh": "underfloor", "rad": "radiators"}


def flat(d):
    if isinstance(d, list): return d
    out = []
    for v in d.values():
        if isinstance(v, list): out += v
    return out


def mean(rows, k):
    v = [float(r[k]) for r in rows if r.get(k) not in (None, "")]
    return round(st.fmean(v), 4) if v else None


def summarise(study, rnd, rows, labeller, metrics):
    g = defaultdict(list)
    for r in rows:
        lab = labeller(r)
        if lab: g[lab].append(r)
    out = []
    for (label, code), L in g.items():
        row = {"study": study, "round": rnd, "setting": label, "code": code, "runs": len(L)}
        for name, key in metrics.items(): row[name] = mean(L, key)
        out.append(row)
    return out


def spec_label(s):
    s = str(s)
    return ("No tuner (fixed curve)" if s == "none" else s), s


HP_METRICS = {"starts_per_day": "starts_per_day", "room_error_c": "mean_abs_err", "out_band_min_d": "out_band_min_d",
              "elec_kwh": "elec_kwh"}
ST_METRICS = {"starts_per_day": "starts_per_day", "room_rms_c": "rms", "out_band_min_d": "out_band_min_d",
              "coeff_changes": "writes", "flips": "flips", "gas_pct": "gas_pct"}


def hp_variant(v, par):
    v = str(v); p = par
    if v == "own": return "Heat pump's own control"
    if v == "pwm": return f"SAT PWM {p} min"
    if v == "pwmlive": return f"SAT PWM {p} min, ON time live"
    if v == "tp": return f"Time-proportional {p} min"
    if v == "dm": return f"Degree minutes -{p}"
    if v == "rh": return f"Room hysteresis {p} C"
    if v == "mr": return f"Minimum run {p} min"
    if v == "fw":
        q = list(p) if isinstance(p, (list, tuple)) else [p]
        s = f"Firmware PWM {q[0]} min, ON {q[1]} min" if len(q) > 1 else f"Firmware PWM {q[0]} min"
        return s + (f", OFF {q[2]} min" if len(q) > 2 else "")
    if v == "sat": return "SAT (PWM switch)"
    if v == "cont": return "SAT continuous"
    if v == "wc": return "Unit's own weather curve"
    return f"{v} {p}"


def build():
    rows = []
    # TASK-1519 curve self-tune
    for n in (1, 2, 3):
        L = [r for r in csv.DictReader(open(f"{T}round{n}_summary.csv")) if r["scope"] == "all"]
        for r in L:
            lab, code = spec_label(r["spec"])
            rows.append({"study": "Curve self-tune", "round": str(n), "setting": lab, "code": code, "runs": int(r["n"]),
                         "starts_per_day": None, "room_rms_c": float(r["rms"]), "out_band_min_d": float(r["minutes_out_of_band_per_day"]),
                         "coeff_changes": float(r["writes"]), "flips": float(r["flips"]),
                         "gas_pct": float(r["gas_pct"]) if r.get("gas_pct") else None})
    r4 = f"{T}round4_results.json"
    if os.path.exists(r4):
        L = flat(json.load(open(r4)))
        rows += summarise("Curve self-tune", "4", L, lambda r: (f"{spec_label(r['spec'])[0]} · {SRC.get(r.get('source'), r.get('source'))}", f"{r['spec']}|{r.get('source')}"), ST_METRICS)
    L = flat(json.load(open(f"{T}round5_results_raw.json")))
    rows += summarise("Curve self-tune", "5", L, lambda r: (f"{spec_label(r['spec'])[0]} · {SRC.get(r.get('source'), r.get('source'))}", f"{r['spec']}|{r.get('source')}"), ST_METRICS)
    for rnd, f in (("5b", "round5b_runs.json"), ("5d", "round5d_runs.json"), ("5e", "round5e_runs.json"),
                   ("5f", "round5f_runs.json"), ("5g", "round5g_runs.json")):
        L = flat(json.load(open(T + f)))
        rows += summarise("Curve self-tune", rnd, L, lambda r: (f"{spec_label(r['spec'])[0]} · {SRC.get(r.get('source'), r.get('source'))}", f"{r['spec']}|{r.get('source')}"), ST_METRICS)
    d5c = json.load(open(T + "round5c_runs.json")); L = flat(d5c["res"]) if isinstance(d5c["res"], (list, dict)) else []
    rows += summarise("Curve self-tune", "5c", L, lambda r: (f"{spec_label(r['spec'])[0]} · {SRC.get(r.get('source'), r.get('source'))}", f"{r['spec']}|{r.get('source')}"), ST_METRICS)
    L = flat(json.load(open(T + "round7_runs.json")))
    rows += summarise("Curve self-tune", "7", L, lambda r: (f"{spec_label(r['spec'])[0]} · signal {r.get('signal')}", f"{r['spec']}|{r.get('signal')}"), ST_METRICS)
    d8 = json.load(open(T + "round8_runs.json"))
    for w, L in d8.items():
        rows += summarise("Curve self-tune", "8", L, lambda r, w=w: (f"{spec_label(r['spec'])[0]} · winter {w.replace('_', '-')}", f"{r['spec']}|{w}"), ST_METRICS)
    L = []
    for f in sorted(glob.glob(T + "round9/*.json")): L += flat(json.load(open(f)))
    rows += summarise("Curve self-tune", "9", L, lambda r: (f"{spec_label(r['spec'])[0]} · {SRC.get(r.get('source'), r.get('source'))}", f"{r['spec']}|{r.get('source')}"), ST_METRICS)
    for rnd, f in (("10", "round10_runs.json"), ("11", "round11_runs.json")):
        for k, L in json.load(open(T + f)).items():
            kk = "winter " + k.replace("_reset", " (PID reset on a step)").replace("_", "-").replace("|", ", margin ") + (" C" if "|" in k else "")
            rows += summarise("Curve self-tune", rnd, L, lambda r, kk=kk, k=k: (f"{spec_label(r['spec'])[0]} · {kk}", f"{r['spec']}|{k}"), ST_METRICS)
    # TASK-1527 heat pumps
    L = flat(json.load(open(H + "round12_hp_onoff_runs.json")))
    rows += summarise("Heat pumps", "12", L, lambda r: (f"{hp_variant(r['variant'], r['period_min'])} · min ON {r['minon_min']} min", f"{r['variant']}|{r['period_min']}|{r['minon_min']}"), HP_METRICS)
    L = flat(json.load(open(H + "round13_hp_longrun_runs.json")))
    rows += summarise("Heat pumps", "13", L, lambda r: (hp_variant(r["variant"], r["par"]), f"{r['variant']}|{r['par']}"), HP_METRICS)
    for rnd, f in (("14", "round14_hp_period_runs.json"), ("15", "round15_hp_season_runs.json")):
        L = flat(json.load(open(H + f)))
        lab = lambda r: ((f"{PLANT[r['plant']]} · {hp_variant(r['variant'], r['par'])}", f"{r['plant']}|{r['variant']}|{r['par']}") if r.get("variant")
                         else (f"{PLANT[r['plant']]} · {SRC.get(r.get('source'))} · SAT PWM {int(r.get('period_s', 0) / 60)} min", f"{r['plant']}|{r.get('source')}|{r.get('period_s')}"))
        rows += summarise("Heat pumps", rnd, L, lab, HP_METRICS)
    L = flat(json.load(open(H + "round16_hp_dm_runs.json")))
    rows += summarise("Heat pumps", "16", L, lambda r: (f"radiators · {SRC.get(r['source'])} · " + ("Heat pump's own control" if r["mode"] == "own" else f"SAT PWM {int(r['par'] / 60)} min" if r["mode"] == "sat" else f"Degree minutes -{r['par']}"), f"{r['mode']}|{r['par']}|{r['source']}"), HP_METRICS)
    L = flat(json.load(open(H + "round17_hp_period_sweep_runs.json")))
    rows += summarise("Heat pumps", "17", L, lambda r: ((f"underfloor · {hp_variant(r['variant'], r['period_min'])}" if r.get("variant") else f"radiators · {SRC.get(r.get('source'))} · SAT PWM {r['period_min']} min"), f"{r.get('plant')}|{r.get('variant')}|{r.get('source')}|{r['period_min']}"), HP_METRICS)
    for rnd, f in (("18b", "round18b_fw_pwm_runs.json"), ("18c", "round18c_minon10_runs.json"), ("18d", "round18d_minoff10_runs.json")):
        L = flat(json.load(open(H + f)))
        rows += summarise("Heat pumps", rnd, L, lambda r: (hp_variant(r["variant"], r["par"]), f"{r['variant']}|{r['par']}"), HP_METRICS)
    L = flat(json.load(open(H + "round19_bestpractice_runs.json")))
    rows += summarise("Heat pumps", "19", L, lambda r: (f"{hp_variant(r['variant'], r['par'])} · {gain(r.get('kp_mode'))}" + setback(r.get("setback")), f"{r['variant']}|{r['par']}|{r.get('kp_mode')}|{r.get('setback')}"), HP_METRICS)
    L = flat(json.load(open(H + "round19b_curve_sweep_runs.json")))
    rows += summarise("Heat pumps", "19b", L, lambda r: (f"{hp_variant(r['variant'], r['par'])} · curve {r.get('coeff')}", f"{r['variant']}|{r['par']}|{r.get('coeff')}"), HP_METRICS)
    L = flat(json.load(open(H + "round19c_tuned_curve_runs.json")))
    rows += summarise("Heat pumps", "19c", L, lambda r: (f"{hp_variant(r['variant'], r['par'])} · curve {r.get('coeff')}", f"{r['variant']}|{r['par']}|{r.get('coeff')}"), HP_METRICS)
    L = flat(json.load(open(H + "round20_elga_runs.json")))
    rows += summarise("Heat pumps", "20", L, lambda r: (f"{hp_variant(r['variant'], None)} · curve {r.get('coeff')}", f"{r['variant']}|{r.get('coeff')}"), HP_METRICS)
    L = flat(json.load(open(H + "round21_library_runs.json")))
    rows += summarise("Heat pumps", "21", L, lambda r: (f"{UNIT[r['unit']]} · {hp_variant(r['variant'], None)}", f"{r['unit']}|{r['variant']}"), HP_METRICS)
    for rnd, f, key in (("23", "round23_candidate_runs.json", "candidate"), ("23b", "round23b_hysteresis_runs.json", "candidate"),
                        ("23c", "round23c_switch_runs.json", "candidate")):
        L = flat(json.load(open(H + f)))
        rows += summarise("Heat pumps", rnd, L, lambda r, key=key: (CAND.get(r[key], r[key]), r[key]), HP_METRICS)
    HB = {"starts_per_day": "boiler_starts", "room_error_c": "mean_abs_err", "gas_kwh": "gas_kwh", "elec_kwh": "elec_kwh"}
    for rnd, f in (("22", "round22_hybrid_backup_runs.json"), ("22b", "round22b_boiler55_runs.json"), ("22c", "round22c_boilermod_runs.json")):
        if not os.path.exists(H + f): continue
        L = flat(json.load(open(H + f)))
        out = summarise("Hybrids", rnd, L, lambda r: (f"{UNIT[r['unit']]} · {PLANT.get(r['plant'], r['plant'])} {r['era'][2:]} · " + ("boiler's own control" if r["hybrid_mode"] == "own" else "slave SAT"), f"{r['unit']}|{r['plant']}|{r['era']}|{r['hybrid_mode']}"), HB)
        for o in out: o["boiler_starts_per_season"] = o.pop("starts_per_day")
        rows += out
    return rows


GH = "https://github.com/rvdbreemen/OTGW-firmware/blob/dev/"
R1519 = "docs/sat/sim-1519/round5b-round7.md"; E1519 = "docs/sat/sim-1519/evaluation-2026-10-31.md"
REPORT_SECTIONS = {   # round: (file, heading prefix of the section, or None for the whole body)
    **{k: (E1519, "## 1. What the simulations show") for k in ("1", "2", "3", "4", "6")},
    "5": (R1519, "## 1. Round 5 result"), "5b": (R1519, "## 3. Round 5b"),
    **{k: (R1519, "## 6. Rounds 5c-5g") for k in ("5c", "5d", "5e", "5f", "5g")},
    "7": (R1519, "## 4. Round 7"), "8": (R1519, "## 8. Round 8"), "9": (R1519, "## 9. Round 9"),
    **{k: (E1519, "## 7. Appendix: rounds 10 and 11") for k in ("10", "11")},
    "12": (H + "round12.md", None), "13": (H + "round13.md", None),
    "14": (H + "round14.md", "## 4. Findings"), "15": (H + "round15.md", "## Findings"),
    "16": (H + "round16.md", "## Findings"), "17": (H + "round17.md", "## Findings"),
    "18b": (H + "round18b.md", "## Findings"), "18c": (H + "round18b.md", "## Round 18c"),
    **{k: (H + "round19.md", "## Findings") for k in ("19", "19b", "19c")},
    "20": (H + "round20.md", "## Findings"), "21": (H + "round21.md", "## Findings"),
    "22": (H + "round22.md", "## Findings"), "22b": (H + "round22b.md", "## Findings"),
    "23": (H + "round23.md", "## Findings"), "23b": (H + "round23.md", "## Hysteresis (round 23b"),
    "23c": (H + "round23.md", "## Round 23c"),
}


def reports():
    out = []
    for rnd, (path, head) in REPORT_SECTIONS.items():
        if not os.path.exists(path): continue
        lines = open(path, encoding="utf-8").read().splitlines()
        title = next((l[2:].strip() for l in lines if l.startswith("# ")), path)
        if head is None:
            body = [l for l in lines if not l.startswith("# ")]; section = "Whole report"
        else:
            i = next((n for n, l in enumerate(lines) if l.startswith(head)), None)
            if i is None: continue
            j = next((n for n in range(i + 1, len(lines)) if lines[n].startswith("## ")), len(lines))
            body = lines[i + 1:j]; section = lines[i].lstrip("# ").strip()
        text = "\n".join(body).strip()
        if len(text) > 9000: text = text[:9000].rsplit("\n", 1)[0] + "\n\n(continued in the full report)"
        shared = [k for k, v in REPORT_SECTIONS.items() if v == (path, head) and k != rnd]
        out.append({"round": rnd, "title": title, "section": section, "text": text, "url": GH + path,
                    "shared_with": ", ".join(shared)})
    return out


if __name__ == "__main__":
    rows = build()
    json.dump(rows, open(sys.argv[1], "w", encoding="utf-8"), indent=0, ensure_ascii=False)
    from collections import Counter
    print(len(rows), "rows;", dict(Counter(r["round"] for r in rows)))
    if len(sys.argv) > 2:
        rep = reports(); json.dump(rep, open(sys.argv[2], "w", encoding="utf-8"), indent=0, ensure_ascii=False)
        print(len(rep), "report sections")
