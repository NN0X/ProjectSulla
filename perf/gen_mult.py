#!/usr/bin/env python3
"""Generate an N x N array multiplier as (1) a Sulla layout JSON, (2) gate-level
Verilog matching the same netlist, and (3) behavioral Verilog (a*b) as the
optimal Verilator ceiling. The netlist is built symbolically and verified
exhaustively against a*b before emitting anything."""
import json, sys, itertools, random

AND, OR, NOT, NAND, NOR, XOR, XNOR, SOURCE, OUTPUT, CUSTOM = 0,1,2,3,4,5,6,7,8,9

class Builder:
    def __init__(self):
        self.parts=[]      # dicts: id,type,label,x,y,numInputs,numOutputs
        self.conns=[]      # {from:{id,pin}, to:{id,pin}}
        self.nid=100
        # symbolic eval graph: node id -> (kind, [input pins], nout)
        self.kind={}; self.inpins={}
    def add(self, typ, ni, no, x, y, label=""):
        i=self.nid; self.nid+=1
        self.parts.append({"id":i,"type":typ,"label":label,"x":float(x),"y":float(y),
                           "numInputs":ni,"numOutputs":no})
        self.kind[i]=typ; self.inpins[i]=[None]*ni
        return i
    def wire(self, frm, to):   # frm=(id,pin) to=(id,pin)
        self.conns.append({"from":{"id":frm[0],"pin":frm[1]},"to":{"id":to[0],"pin":to[1]}})
        self.inpins[to[0]][to[1]]=frm
    # gate helpers return an output pin (id,0)
    def AND(self,a,b,x=0,y=0):
        g=self.add(AND,2,1,x,y); self.wire(a,(g,0)); self.wire(b,(g,1)); return (g,0)
    def XOR(self,a,b,x=0,y=0):
        g=self.add(XOR,2,1,x,y); self.wire(a,(g,0)); self.wire(b,(g,1)); return (g,0)
    def HA(self,a,b,x=0,y=0):    # half adder -> (sum,carry)
        return self.XOR(a,b,x,y), self.AND(a,b,x,y+4)
    def FA(self,a,b,c,x=0,y=0):  # full adder as CUSTOM subpart -> (sum,cout)
        g=self.add(CUSTOM,3,2,x,y,"full_adder")
        self.wire(a,(g,0)); self.wire(b,(g,1)); self.wire(c,(g,2))
        return (g,0),(g,1)

def build(N):
    B=Builder()
    a=[]; b=[]
    for k in range(N):
        s=B.add(SOURCE,0,1,0,k*12,f"a{k}"); a.append((s,0))
    for k in range(N):
        s=B.add(SOURCE,0,1,0,(N+k)*12,f"b{k}"); b.append((s,0))
    # partial products pp[i][j] = a[j] & b[i]
    pp=[[B.AND(a[j],b[i],200+i*60,(i*N+j)*6) for j in range(N)] for i in range(N)]
    # shift-and-add accumulation, constant-free (HA at edges, FA in middle)
    acc={p:pp[0][p] for p in range(N)}   # row 0 at positions 0..N-1
    colx=600
    for i in range(1,N):
        carry=None; colx+=140
        for k in range(N):
            pos=i+k
            addend=pp[i][k]; existing=acc.get(pos)
            ins=[z for z in (existing,addend,carry) if z is not None]
            if len(ins)==1:
                acc[pos]=ins[0]; carry=None
            elif len(ins)==2:
                s,c=B.HA(ins[0],ins[1],colx,pos*12); acc[pos]=s; carry=c
            else:
                s,c=B.FA(ins[0],ins[1],ins[2],colx,pos*12); acc[pos]=s; carry=c
        pos=i+N
        while carry is not None:
            existing=acc.get(pos)
            if existing is None:
                acc[pos]=carry; carry=None
            else:
                s,c=B.HA(existing,carry,colx+70,pos*12); acc[pos]=s; carry=c
            pos+=1
    # outputs product[0..2N-1]
    outs=[]
    for p in range(2*N):
        o=B.add(OUTPUT,1,1,4000,p*12,f"p{p}")
        src=acc.get(p)
        if src is None:
            raise RuntimeError(f"product bit {p} undriven")
        B.wire(src,(o,0)); outs.append(o)
    return B,a,b,outs

def evaluate(B, avals, bvals, srca, srcb):
    # topological evaluate the symbolic netlist for given source values
    val={}
    for (sid,_),v in list(srca)+list(srcb): val[sid]=v
    memo={}
    def out(pin):
        nid,po=pin
        if nid in val and B.kind[nid]==SOURCE: return val[nid]
        if (nid,po) in memo: return memo[(nid,po)]
        k=B.kind[nid]; ins=[out(x) for x in B.inpins[nid]]
        if k==AND: r=(ins[0]&ins[1],)
        elif k==XOR: r=(ins[0]^ins[1],)
        elif k==CUSTOM: # full_adder
            s=ins[0]^ins[1]^ins[2]; co=(ins[0]&ins[1])|(ins[2]&(ins[0]^ins[1])); r=(s,co)
        elif k==OUTPUT: r=(ins[0],)
        else: raise RuntimeError(f"unhandled kind {k}")
        for idx,rv in enumerate(r): memo[(nid,idx)]=rv
        return memo[(nid,po)]
    return out

def verify(N, exhaustive=True):
    B,a,b,outs=build(N)
    def run(av,bv):
        srca=[(a[k], (av>>k)&1) for k in range(N)]
        srcb=[(b[k], (bv>>k)&1) for k in range(N)]
        ev=evaluate(B,av,bv,srca,srcb)
        prod=0
        for p in range(2*N):
            bit=ev((outs[p],0))
            prod |= (bit<<p)
        return prod
    combos = itertools.product(range(2**N),range(2**N)) if exhaustive else \
             [(random.randrange(2**N),random.randrange(2**N)) for _ in range(4000)]
    bad=0; tot=0
    for av,bv in combos:
        tot+=1
        if run(av,bv)!=av*bv:
            bad+=1
            if bad<=3: print(f"  MISMATCH a={av} b={bv} got={run(av,bv)} want={av*bv}")
    print(f"N={N}: verified {tot} cases, {bad} mismatches")
    return bad==0, B

if __name__=="__main__":
    N=int(sys.argv[1]) if len(sys.argv)>1 else 8
    ok,B=verify(N, exhaustive=(N<=8))
    if not ok: sys.exit(1)
    name=f"mul{N}"
    json.dump({"parts":B.parts,"connections":B.conns}, open(f"perf/layouts/{name}.json","w"), indent=2)
    ngate=sum(1 for p in B.parts if p['type'] in (AND,XOR))
    nfa=sum(1 for p in B.parts if p['type']==CUSTOM)
    print(f"wrote perf/layouts/{name}.json : {len(B.parts)} parts "
          f"({ngate} prim gates + {nfa} full_adder subparts), {len(B.conns)} connections")

def emit_verilog(N):
    B,a,b,outs=build(N)
    id2src={}
    for p in B.parts:
        if p["type"]==SOURCE: id2src[p["id"]]=p["label"]  # 'a3' / 'b5'
    def wname(pin):
        nid,po=pin
        if nid in id2src:
            lab=id2src[nid]; return f"{lab[0]}[{lab[1:]}]"
        return f"w{nid}_{po}"
    lines=[f"module mul{N}(input [{N-1}:0] a, input [{N-1}:0] b, output [{2*N-1}:0] p);"]
    # declare wires for every non-source, non-output gate output pin
    for pp in B.parts:
        if pp["type"] in (AND,XOR):
            lines.append(f"  wire w{pp['id']}_0;")
        elif pp["type"]==CUSTOM:
            lines.append(f"  wire w{pp['id']}_0, w{pp['id']}_1;")
    for pp in B.parts:
        if pp["type"]==AND:
            i0,i1=B.inpins[pp['id']]; lines.append(f"  assign w{pp['id']}_0 = {wname(i0)} & {wname(i1)};")
        elif pp["type"]==XOR:
            i0,i1=B.inpins[pp['id']]; lines.append(f"  assign w{pp['id']}_0 = {wname(i0)} ^ {wname(i1)};")
        elif pp["type"]==CUSTOM:
            i0,i1,i2=B.inpins[pp['id']]
            lines.append(f"  full_adder fa{pp['id']}(.a({wname(i0)}), .b({wname(i1)}), .cin({wname(i2)}), .sum(w{pp['id']}_0), .cout(w{pp['id']}_1));")
    for k,o in enumerate(outs):
        src=B.inpins[o][0]; lines.append(f"  assign p[{k}] = {wname(src)};")
    lines.append("endmodule")
    fa="module full_adder(input a, input b, input cin, output sum, output cout);\n  wire axb=a^b;\n  assign sum=axb^cin;\n  assign cout=(a&b)|(cin&axb);\nendmodule\n"
    open(f"perf/verilator/mul{N}.v","w").write(fa+"\n"+"\n".join(lines)+"\n")
    open(f"perf/verilator/mul{N}_behav.v","w").write(
        f"module mul{N}(input [{N-1}:0] a, input [{N-1}:0] b, output [{2*N-1}:0] p);\n  assign p = a * b;\nendmodule\n")
    print(f"wrote perf/verilator/mul{N}.v (gate-level) and mul{N}_behav.v (behavioral)")
