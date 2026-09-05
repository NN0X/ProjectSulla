#!/usr/bin/env python3
"""Stateful-subpart benchmark for Step 8 (per-instance state, TODO#7).
dff: an enabled D-register  Q_next = EN ? D : Q  (Q holds via feedback -> STATEFUL).
regbankN: N independent dff instances (inputs D[N],EN[N] -> Q[N]). Multiple
instances of a stateful subpart is exactly what per-instance state must get right:
each register must keep its OWN Q. Emits Sulla JSON + gate-level RTL Verilog."""
import json, sys, random
AND,OR,NOT,NAND,NOR,XOR,XNOR,SOURCE,OUTPUT,CUSTOM=0,1,2,3,4,5,6,7,8,9

def dff_layout():
    # inputs: D (y=0), EN (y=10) ; output Q
    parts=[]; conns=[]
    def add(i,t,ni,no,x,y,lab=""): parts.append({"id":i,"type":t,"label":lab,"x":float(x),"y":float(y),"numInputs":ni,"numOutputs":no})
    def w(f,fp,t,tp): conns.append({"from":{"id":f,"pin":fp},"to":{"id":t,"pin":tp}})
    add(100,SOURCE,0,1,0,0,"D"); add(101,SOURCE,0,1,0,10,"EN")
    add(102,AND,2,1,100,0,"d_and_en")     # D & EN
    add(103,NOT,1,1,100,20,"n_en")        # ~EN
    add(104,AND,2,1,200,20,"q_and_nen")   # Q & ~EN  (Q feedback)
    add(105,OR,2,1,300,10,"Q")            # Q = (D&EN)|(Q&~EN)
    add(106,OUTPUT,1,1,400,10,"Qout")
    w(100,0,102,0); w(101,0,102,1)        # d_and_en
    w(101,0,103,0)                        # n_en
    w(105,0,104,0); w(103,0,104,1)        # q_and_nen (reads Q=105)
    w(102,0,105,0); w(104,0,105,1)        # Q
    w(105,0,106,0)                        # output
    return {"parts":parts,"connections":conns}

def regbank(N):
    parts=[]; conns=[]; nid=100
    def add(t,ni,no,x,y,lab=""):
        nonlocal nid; i=nid; nid+=1
        parts.append({"id":i,"type":t,"label":lab,"x":float(x),"y":float(y),"numInputs":ni,"numOutputs":no}); return i
    def w(f,t,tp=0,fp=0): conns.append({"from":{"id":f,"pin":fp},"to":{"id":t,"pin":tp}})
    D=[add(SOURCE,0,1,0,i*8,f"D{i}") for i in range(N)]
    EN=[add(SOURCE,0,1,0,(N+i)*8,f"EN{i}") for i in range(N)]
    Q=[]
    for i in range(N):
        c=add(CUSTOM,2,1,300,i*8,"dff")       # dff instance: pins 0=D,1=EN -> 0=Q
        w(D[i],c,0); w(EN[i],c,1)
        Q.append(c)
    for i in range(N):
        o=add(OUTPUT,1,1,600,i*8,f"Q{i}"); w(Q[i],o,0)
    return {"parts":parts,"connections":conns}

# ---- golden sequential model ----
def golden_step(state, D, EN):   # state,D,EN,Q are int bitmasks over N
    return (D & EN) | (state & ~EN)

def emit_verilog_dff():
    return ("module dff(input clk, input D, input EN, output Q);\n"
            "  reg q=0; assign Q=q;\n"
            "  always @(posedge clk) if (EN) q<=D;\n"   # enabled D-FF
            "endmodule\n")

def emit_verilog_regbank(N):
    L=[f"module regbank{N}(input clk, input [{N-1}:0] D, input [{N-1}:0] EN, output [{N-1}:0] Q);"]
    for i in range(N):
        L.append(f"  dff u{i}(.clk(clk), .D(D[{i}]), .EN(EN[{i}]), .Q(Q[{i}]));")
    L.append("endmodule")
    return emit_verilog_dff()+"\n"+"\n".join(L)+"\n"

if __name__=="__main__":
    import os, shutil
    json.dump(dff_layout(), open("layouts/dff.json","w"), indent=2)
    print("wrote layouts/dff.json (stateful enabled D-register)")
    for N in (8, 64):
        json.dump(regbank(N), open(f"layouts/regbank{N}.json","w"), indent=2)
        p=len(json.load(open(f"layouts/regbank{N}.json"))["parts"])
        print(f"wrote layouts/regbank{N}.json: {p} parts ({N} dff instances)")
        open(f"verilator/regbank{N}.v","w").write(emit_verilog_regbank(N))
    print("wrote verilator/regbank{8,64}.v")
    # Validation (tests/validate.cpp) runs from tests/ and reads its own layouts/.
    # dff + regbank8 are the per-instance-state fixtures, so mirror them there.
    td="../tests/layouts"
    if os.path.isdir(td):
        for f in ("dff.json","regbank8.json"):
            shutil.copyfile(f"layouts/{f}", f"{td}/{f}")
        print(f"copied dff.json, regbank8.json -> {td}/")
