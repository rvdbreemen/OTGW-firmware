import sys, statistics as st
sys.path.insert(0, r"D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware/scripts")
from multiprocessing import Pool
import sat_curve_selftune_study as base, sat_curve_selftune_study4 as s4
W = r"C:/Users/rvdbr/AppData/Local/Temp/claude/D--Users-Robert-Documents-GitHub-RvdB-OTGW-firmware/7e8a9b2c-8154-4fbc-badd-02c669e54188/scratchpad/curvesim/weather_2025_26.json"
def init(excl):
    base._init(W); s4.EXCLUDE_PWM = excl
def job(a): return s4.run(a)
if __name__ == "__main__":
    jobs = [(spec, era, "heavy", 1.0, off, 30, 1, False, src) for spec in [("none",), ("D", 3, 1.0, 0.1, False)]
            for era in ("3_1983-92", "4_1992-2012", "5_2015", "6_BENG2021") for off in (-0.5, 0.0, 0.5) for src in ("gas", "hp30")]
    for excl in (False, True):
        with Pool(6, initializer=init, initargs=(excl,)) as p: res = p.map(job, jobs)
        for src in ("gas", "hp30"):
            for spec in ("none", "D 3d thr1.0 st0.1"):
                rs = [r for r in res if r["source"] == src and r["spec"] == spec]
                f = lambda k: st.fmean(r[k] for r in rs)
                print(f"exclude_pwm={excl!s:5s} {src:5s} {spec:18s} rms {f('rms'):.3f} out {f('out_band_min_d'):5.0f} over {f('over_min_d'):5.0f} writes {f('writes'):5.1f} flips {f('flips'):4.1f}")
