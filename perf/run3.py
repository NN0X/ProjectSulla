#!/usr/bin/env python3
# Run perf/bench 3x, parse the CSV block, report per-(circuit,mode) median ns/tick,
# and show it beside the Verilator reference and the committed pre-optim baseline.
import subprocess, statistics, sys, os, csv
os.chdir(os.path.dirname(os.path.abspath(__file__)))
runs=[]
for r in range(3):
    out=subprocess.run(["./bench"],capture_output=True,text=True).stdout
    rows={}
    incsv=False
    for line in out.splitlines():
        if line.strip()=="-- csv --" or line.strip().endswith("-- csv --"): incsv=True; continue
        if incsv and "," in line and not line.startswith("circuit,"):
            p=line.split(",")
            if len(p)>=6:
                rows[(p[0],p[3])]=float(p[5])
    runs.append(rows)
keys=sorted(runs[0].keys(), key=lambda k:(["full_adder","adder8","adder16","adder32"].index(k[0]) if k[0] in ["full_adder","adder8","adder16","adder32"] else 9, ["interpreted","native-inline","native-link","native-raw"].index(k[1]) if k[1] in ["interpreted","native-inline","native-link","native-raw"] else 9))
med={k:statistics.median([run[k] for run in runs if k in run]) for k in keys}
# verilator ref
vref={}
vp="verilator/verilator_ref.csv"
if os.path.exists(vp):
    for row in csv.DictReader(open(vp)): vref[row["circuit"]]=float(row["verilator_ns_per_tick"])
# print
print("\n== median-of-3 ns/tick (lower is better) ==")
print(f"{'circuit':<11}{'mode':<15}{'median_ns':>10}{'verilator':>11}{'vs_veri':>9}")
for k in keys:
    c,m=k
    v=vref.get(c)
    ratio = (med[k]/v) if v else float('nan')
    vs = f"{ratio:.2f}x" if v else "-"
    vshow = f"{v:.2f}" if (v and m!='interpreted') else "-"
    print(f"{c:<11}{m:<15}{med[k]:>10.2f}{vshow:>11}{vs:>9}")
# save
with open("perf_median3.csv","w") as f:
    f.write("circuit,mode,median_ns_per_tick\n")
    for k in keys: f.write(f"{k[0]},{k[1]},{med[k]:.2f}\n")
print("\nsaved perf/perf_median3.csv")
