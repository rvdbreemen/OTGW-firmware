#!/usr/bin/env python3
"""Check SAT PID gain/rounding formulas on /api/v2/sat/status samples (JSON lines).

Input: JSON lines with the sat status fields kp, ki, kd, pid_p, pid_i, pid_d, pid_output, heating_curve, error,
coefficient (a sampler like solarA3.sh writes them; add these names to its field list).
Formulas (coordinator, entries 480/482/490), checked with a tolerance of one unit in the last rounded place:
  ki = round6(kp / 8400)          kd = round6(0.04 * 8400 * kp)
  P  = round3(kp * error)         D  = round3(kd * rawDerivative)  (raw derivative not in the status: only checked for 3 decimals)
  output = round1(curve + P + I + D)    and the output has at most 1 decimal
Prints up to 5 sample lines with the hand-computed values, then a pass/fail count per rule. No NaN allowed anywhere.
Use: python -I check_pid_formulas.py samples.jsonl
Not run on a device yet (waits for the candidate build and Robert's yes for the sim run).
"""
import json, math, sys


def dec(v, n):
    return v is not None and abs(round(v, n) - v) < 10 ** -(n + 3)


def main(path):
    rows = [json.loads(l) for l in open(path, encoding="utf-8") if l.strip()]
    rows = [r for r in rows if r.get("kp") and r.get("pid_output") is not None]  # kp 0 = SAT idle / sim off: nothing to check
    res = {"ki": [0, 0], "kd": [0, 0], "P": [0, 0], "out_1dec": [0, 0], "out_sum": [0, 0], "nan": [0, 0]}

    def chk(k, ok):
        res[k][0 if ok else 1] += 1

    shown = 0
    step = max(1, len(rows) // 5)
    for idx, r in enumerate(rows):
        kp, ki, kd = r["kp"], r["ki"], r["kd"]
        vals = [r.get(k) for k in ("kp", "ki", "kd", "pid_p", "pid_i", "pid_d", "pid_output")]
        chk("nan", all(v is not None and not math.isnan(v) and not math.isinf(v) for v in vals))
        eki, ekd = round(kp / 8400, 6), round(0.04 * 8400 * kp, 6)
        chk("ki", abs(ki - eki) <= 1.5e-6)
        chk("kd", abs(kd - ekd) <= max(1.5e-6, 1e-6 * abs(ekd)))  # kd is a float32 in the REST output
        if r.get("error") is not None:
            ep = round(kp * r["error"], 3)
            chk("P", abs(r["pid_p"] - ep) <= 1.5e-3)
        chk("out_1dec", dec(r["pid_output"], 1))
        if r.get("heating_curve") is not None:
            s = round(r["heating_curve"] + r["pid_p"] + r["pid_i"] + r["pid_d"], 1)
            chk("out_sum", abs(r["pid_output"] - s) <= 0.11)
        if shown < 5 and idx % step == 0 and r.get("error") is not None:
            shown += 1
            print(f"{r.get('t')} kp={kp} ki={ki} (calc {eki}) kd={kd} (calc {ekd}) err={r['error']} P={r['pid_p']} (calc {round(kp*r['error'],3)}) "
                  f"I={r['pid_i']} D={r['pid_d']} curve={r.get('heating_curve')} out={r['pid_output']}")
    for k, (ok, bad) in res.items():
        print(f"{k}: ok {ok} bad {bad}")
    return 1 if any(b for _, b in res.values()) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
