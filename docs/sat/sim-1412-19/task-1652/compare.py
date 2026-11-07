import json, sys, statistics as st, os
SP=sys.argv[1]; REPO=sys.argv[2]
CL=("very_low_load","thermal_lag","minimum_power","trv_pump","dhw","ignition","room_fault","outside_fault","transport")
def load(d):
    rs=[]
    for c in CL: rs+=json.load(open(os.path.join(SP,d,"parts",c+".json")))
    return rs
key=lambda r:(r["class"],r["era"],r["emitter"],r["flow"],r["pmin_kw"],r["mm_honoured"],r["seed"])
LB=load("sim-base"); LR=load("sim-rule"); LC=json.load(open(os.path.join(REPO,"docs/sat/sim-1412-19/runs.json")))["runs"]
LC=[r for c in CL for r in LC if r["class"]==c]
assert [key(r) for r in LB]==[key(r) for r in LR]==[key(r) for r in LC]
B=dict(enumerate(LB)); R=dict(enumerate(LR)); C=dict(enumerate(LC)); key=lambda r:r["class"]
# precondition: baseline == committed
diff=0
for k,r in B.items():
    a=dict(r); a.pop("S6_short_cause",None)
    if a!=C[k]: diff+=1
print("baseline runs", len(B), "differ from committed runs.json:", diff)
tot=lambda D,c:sum(r["S6_short_cause"].get(c,0) for r in D.values())
for c in ("cold_cutoff","cold_cutoff_flame_out","on_timer","pwm_exit"):
    print("short phases cause", c, "base", tot(B,c), "rule", tot(R,c))
print("S6 short total base", sum(r["S6_short_on"] for r in B.values()), "rule", sum(r["S6_short_on"] for r in R.values()))
hard_new=[k for k in B if any(B[k]["pass"][h] and not R[k]["pass"][h] for h in ("H1","H2","H3","H4"))]
s2_new=[k for k in B if B[k]["pass"]["S2"] and not R[k]["pass"]["S2"]]
print('runs newly failing a hard check:', len(hard_new)); print('runs newly failing S2:', len(s2_new))
changed=sum(1 for k in B if {x:v for x,v in B[k].items() if x!="S6_short_cause"}!={x:v for x,v in R[k].items() if x!="S6_short_cause"})
print("runs with any metric change:", changed)
print()
print("| class | runs | changed | cold-cut short base/rule | flame-out at cut base/rule | S6 short base/rule | S6 pass base/rule | starts/day med base/rule (worst) | S2 max-over med base/rule (worst d) | S1 med base/rule | S3 worst base/rule | S2 pass base/rule | S4 pass base/rule | hard fails base/rule |")
print("|"+"---|"*15)
for c in CL:
    ks=[k for k in B if B[k]['class']==c]; n=len(ks)
    def share(D,sk):
        app=[D[k] for k in ks if D[k]["pass"][sk] is not None]; return "-" if not app else "%d%%"%round(100*sum(1 for r in app if r["pass"][sk])/len(app))
    ch=sum(1 for k in ks if {x:v for x,v in B[k].items() if x!="S6_short_cause"}!={x:v for x,v in R[k].items() if x!="S6_short_cause"})
    cc=lambda D,cause:sum(D[k]["S6_short_cause"].get(cause,0) for k in ks)
    s6=lambda D:sum(D[k]["S6_short_on"] for k in ks)
    med=lambda D,m:st.median(D[k][m] for k in ks)
    dS2=[R[k]["S2_max_over"]-B[k]["S2_max_over"] for k in ks]; dS4=[R[k]["S4_starts_per_day"]-B[k]["S4_starts_per_day"] for k in ks]
    hf=lambda D:sum(1 for k in ks if not all(D[k]["pass"][h] for h in ("H1","H2","H3","H4")))
    print(f"| {c} | {n} | {ch} | {cc(B,'cold_cutoff')}/{cc(R,'cold_cutoff')} | {cc(B,'cold_cutoff_flame_out')}/{cc(R,'cold_cutoff_flame_out')} | {s6(B)}/{s6(R)} | {share(B,'S6')}/{share(R,'S6')} | {med(B,'S4_starts_per_day'):.1f}/{med(R,'S4_starts_per_day'):.1f} (d max {max(dS4):+.2f}, min {min(dS4):+.2f}) | {med(B,'S2_max_over'):.3f}/{med(R,'S2_max_over'):.3f} (d max {max(dS2):+.3f}) | {med(B,'S1_share_in_0_3'):.3f}/{med(R,'S1_share_in_0_3'):.3f} | {min(B[k]['S3_max_under'] for k in ks):.2f}/{min(R[k]['S3_max_under'] for k in ks):.2f} | {share(B,'S2')}/{share(R,'S2')} | {share(B,'S4')}/{share(R,'S4')} | {hf(B)}/{hf(R)} |")
