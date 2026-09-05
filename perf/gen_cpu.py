#!/usr/bin/env python3
"""cpu8: 8-bit single-cycle accumulator CPU core.
State (feedback registers): ACC[8], PC[8], CARRY. Inputs: opcode[3], imm[8].
Ops: 0 NOP,1 LDI,2 ADD,3 AND,4 OR,5 XOR,6 NOT,7 ADDC. PC increments each tick.
Combinational next-state is verified against a golden ALU (registers as free
'prev' inputs) before emitting Sulla JSON + gate-level RTL Verilog."""
import json, sys, random
AND,OR,NOT,NAND,NOR,XOR,XNOR,SOURCE,OUTPUT,CUSTOM=0,1,2,3,4,5,6,7,8,9
W=8

class B:
    def __init__(s):
        s.parts=[]; s.conns=[]; s.nid=100; s.kind={}; s.ins={}; s.reg=set()
    def add(s,t,ni,no,x,y,lab=""):
        i=s.nid; s.nid+=1
        s.parts.append({"id":i,"type":t,"label":lab,"x":float(x),"y":float(y),"numInputs":ni,"numOutputs":no})
        s.kind[i]=t; s.ins[i]=[None]*ni; return i
    def w(s,f,t): s.conns.append({"from":{"id":f[0],"pin":f[1]},"to":{"id":t[0],"pin":t[1]}}); s.ins[t[0]][t[1]]=f
    def NOT(s,a): g=s.add(NOT,1,1,0,0); s.w(a,(g,0)); return (g,0)
    def AND(s,a,b): g=s.add(AND,2,1,0,0); s.w(a,(g,0)); s.w(b,(g,1)); return (g,0)
    def OR(s,a,b):  g=s.add(OR,2,1,0,0); s.w(a,(g,0)); s.w(b,(g,1)); return (g,0)
    def XOR(s,a,b): g=s.add(XOR,2,1,0,0); s.w(a,(g,0)); s.w(b,(g,1)); return (g,0)
    def XNOR(s,a,b):g=s.add(XNOR,2,1,0,0);s.w(a,(g,0)); s.w(b,(g,1)); return (g,0)
    def ANDn(s,xs): g=s.add(AND,len(xs),1,0,0); [s.w(p,(g,k)) for k,p in enumerate(xs)]; return (g,0)
    def NORn(s,xs): g=s.add(NOR,len(xs),1,0,0); [s.w(p,(g,k)) for k,p in enumerate(xs)]; return (g,0)
    def HA(s,a,b): return s.XOR(a,b), s.AND(a,b)
    def FA(s,a,b,c,x=0,y=0):
        g=s.add(CUSTOM,3,2,x,y,"full_adder"); s.w(a,(g,0)); s.w(b,(g,1)); s.w(c,(g,2)); return (g,0),(g,1)

def build():
    b=B()
    op =[(b.add(SOURCE,0,1,0,i*10,f"op{i}"),0) for i in range(3)]
    imm=[(b.add(SOURCE,0,1,0,40+i*10,f"im{i}"),0) for i in range(W)]
    # register nodes (mux outputs / counter nodes) — created first so ops can reference prev
    ACC=[(b.add(OR,8,1,300,i*20,f"acc{i}"),0) for i in range(W)]
    CARRY=(b.add(OR,3,1,300,200,"carry"),0)
    PC =[(b.add(XOR,2,1,700,i*20,f"pc{i}"),0) for i in range(W)]
    for x,_ in ACC+PC+[CARRY]: b.reg.add(x)
    # decoder 3->8 one-hot
    nop=[b.NOT(op[i]) for i in range(3)]
    onehot=[b.ANDn([op[i] if (j>>i)&1 else nop[i] for i in range(3)]) for j in range(8)]
    # ALU
    s0,carry=b.HA(ACC[0],imm[0]); add_r=[s0]
    for i in range(1,W):
        si,carry=b.FA(ACC[i],imm[i],carry,500,i*20); add_r.append(si)
    add_cout=carry
    carry=CARRY; addc_r=[]
    for i in range(W):
        si,carry=b.FA(ACC[i],imm[i],carry,560,i*20); addc_r.append(si)
    addc_cout=carry
    and_r=[b.AND(ACC[i],imm[i]) for i in range(W)]
    or_r =[b.OR (ACC[i],imm[i]) for i in range(W)]
    xor_r=[b.XOR(ACC[i],imm[i]) for i in range(W)]
    not_r=[b.NOT(ACC[i]) for i in range(W)]
    res=lambda i:[ACC[i],imm[i],add_r[i],and_r[i],or_r[i],xor_r[i],not_r[i],addc_r[i]]  # 0 NOP=hold
    for i in range(W):
        for j in range(8):
            b.w(b.AND(res(i)[j],onehot[j]),(ACC[i][0],j))
    # CARRY_next
    isc=b.OR(onehot[2],onehot[7])
    b.w(b.AND(add_cout,onehot[2]),(CARRY[0],0))
    b.w(b.AND(addc_cout,onehot[7]),(CARRY[0],1))
    b.w(b.AND(CARRY,b.NOT(isc)),(CARRY[0],2))
    # PC free-running increment (bit0 toggles via constant ONE = XNOR(op0,op0))
    ONE=b.XNOR(op[0],op[0]); pcarry=ONE
    for i in range(W):
        b.w(PC[i],(PC[i][0],0)); b.w(pcarry,(PC[i][0],1))
        if i<W-1: pcarry=b.AND(PC[i],pcarry)
    ZERO=b.NORn([ACC[i] for i in range(W)])
    outs=[]; y=0
    for i in range(W): o=b.add(OUTPUT,1,1,1200,y,f"a{i}"); b.w(ACC[i],(o,0)); outs.append(o); y+=10
    for i in range(W): o=b.add(OUTPUT,1,1,1200,y,f"p{i}"); b.w(PC[i],(o,0)); outs.append(o); y+=10
    o=b.add(OUTPUT,1,1,1200,y,"cy"); b.w(CARRY,(o,0)); outs.append(o); y+=10
    o=b.add(OUTPUT,1,1,1200,y,"zr"); b.w(ZERO,(o,0)); outs.append(o)
    return b,op,imm,ACC,PC,CARRY,outs

def make_eval(b,op,imm,ACC,PC,CARRY):
    """operand()=prev for registers; next[reg]=gate evaluated over operands."""
    def evaluate(opv,immv,accv,pcv,cyv):
        srcval={}
        for p in b.parts:
            if p["type"]==SOURCE:
                l=p["label"]
                if l.startswith("op"): srcval[p["id"]]=(opv>>int(l[2:]))&1
                elif l.startswith("im"): srcval[p["id"]]=(immv>>int(l[2:]))&1
        prev={}
        for i,(x,_) in enumerate(ACC): prev[x]=(accv>>i)&1
        for i,(x,_) in enumerate(PC):  prev[x]=(pcv>>i)&1
        prev[CARRY[0]]=cyv&1
        memo={}
        def gate(nid):
            k=b.kind[nid]; vs=[operand(p) for p in b.ins[nid]]
            if k==AND: r=1
            if k==AND:
                r=1
                for v in vs: r&=v
                return (r,)
            if k==OR:
                r=0
                for v in vs: r|=v; 
                return (r,)
            if k==NOR:
                r=0
                for v in vs: r|=v
                return (1-r,)
            if k==XOR:
                r=0
                for v in vs: r^=v
                return (r,)
            if k==XNOR:
                r=0
                for v in vs: r^=v
                return (1-r,)
            if k==NOT: return (1-vs[0],)
            if k==CUSTOM:
                a,bb,c=vs; return (a^bb^c,(a&bb)|(c&(a^bb)))
            if k==OUTPUT: return (vs[0],)
            raise RuntimeError(k)
        def operand(pin):
            nid,po=pin
            if b.kind[nid]==SOURCE: return srcval[nid]
            if nid in prev: return prev[nid]              # register read as prev
            if (nid,po) in memo: return memo[(nid,po)]
            r=gate(nid)
            for idx,v in enumerate(r): memo[(nid,idx)]=v
            return memo[(nid,po)]
        def nextval(pin):   # compute the register/output node itself
            nid,po=pin; r=gate(nid)
            return r[po]
        acc_n=sum(nextval(ACC[i])<<i for i in range(W))
        pc_n =sum(nextval(PC[i])<<i for i in range(W))
        cy_n =nextval(CARRY)
        return acc_n,pc_n,cy_n
    return evaluate

def golden(opv,immv,acc,pc,cy):
    M=(1<<W)-1
    an={0:acc,1:immv,2:(acc+immv)&M,3:acc&immv,4:acc|immv,5:acc^immv,6:(~acc)&M,7:(acc+immv+cy)&M}[opv]
    cyn=1 if (opv==2 and acc+immv>M) or (opv==7 and acc+immv+cy>M) else (0 if opv in(2,7) else cy)
    return an,(pc+1)&M,cyn

if __name__=="__main__":
    b,op,imm,ACC,PC,CARRY,outs=build()
    ev=make_eval(b,op,imm,ACC,PC,CARRY)
    random.seed(1); bad=0; tot=0
    for _ in range(4000):
        opv=random.randint(0,7); immv=random.randint(0,255); acc=random.randint(0,255); pc=random.randint(0,255); cy=random.randint(0,1)
        g=ev(opv,immv,acc,pc,cy); w=golden(opv,immv,acc,pc,cy); tot+=1
        if g!=w:
            bad+=1
            if bad<=6: print(f"  MISMATCH op={opv} imm={immv} acc={acc} cy={cy}: got={g} want={w}")
    print(f"cpu8 combinational datapath: {tot} cases, {bad} mismatches")
    if bad==0:
        json.dump({"parts":b.parts,"connections":b.conns},open("perf/layouts/cpu8.json","w"),indent=2)
        nfa=sum(1 for p in b.parts if p['type']==CUSTOM); ng=len(b.parts)-nfa-sum(1 for p in b.parts if p['type'] in (SOURCE,OUTPUT))
        print(f"wrote perf/layouts/cpu8.json: {len(b.parts)} parts ({ng} gates + {nfa} full_adder subparts), {len(b.conns)} conns")

def emit_verilog():
    b,op,imm,ACC,PC,CARRY,outs=build()
    regids={ACC[i][0]:("acc",i) for i in range(W)}
    regids.update({PC[i][0]:("pc",i) for i in range(W)})
    regids[CARRY[0]]=("carry",0)
    srcname={}
    for p in b.parts:
        if p["type"]==SOURCE:
            l=p["label"]; srcname[p["id"]]= (f"op[{l[2:]}]" if l.startswith("op") else f"imm[{l[2:]}]")
    def wn(pin):
        nid,po=pin
        if nid in srcname: return srcname[nid]
        if nid in regids: n,i=regids[nid]; return (f"{n}[{i}]" if n!="carry" else "carry")   # register READ = current reg
        return f"w{nid}_{po}"
    L=["module cpu8(input clk, input [2:0] op, input [7:0] imm,",
       "  output [7:0] acc, output [7:0] pc, output carry, output zero);",
       "  reg [7:0] acc_r=0, pc_r=0; reg carry_r=0;",
       "  assign acc=acc_r; assign pc=pc_r; assign carry=carry_r;"]
    # NOTE: register READS map to *_r (current). Redefine wn for reg reads -> *_r
    def wnr(pin):
        nid,po=pin
        if nid in srcname: return srcname[nid]
        if nid in regids:
            n,i=regids[nid]; return f"acc_r[{i}]" if n=="acc" else (f"pc_r[{i}]" if n=="pc" else "carry_r")
        return f"w{nid}_{po}"
    # declare wires for every non-source, non-register gate output
    for p in b.parts:
        if p["type"] in (AND,OR,XOR,XNOR,NOR,NOT):
            if p["id"] in regids: continue
            L.append(f"  wire w{p['id']}_0;")
        elif p["type"]==CUSTOM:
            L.append(f"  wire w{p['id']}_0, w{p['id']}_1;")
    # next-state wires for registers
    L.append("  wire [7:0] acc_n, pc_n; wire carry_n;")
    def expr(nid):
        k=b.kind[nid]; ins=[wnr(p) for p in b.ins[nid]]
        if k==AND: return " & ".join(ins)
        if k==OR:  return " | ".join(ins)
        if k==XOR: return " ^ ".join(ins)
        if k==NOR: return "~(" + " | ".join(ins) + ")"
        if k==XNOR:return "~(" + " ^ ".join(ins) + ")"
        if k==NOT: return "~" + ins[0]
        return None
    # emit combinational assigns for non-register logic gates
    for p in b.parts:
        if p["id"] in regids: continue
        if p["type"] in (AND,OR,XOR,XNOR,NOR,NOT):
            L.append(f"  assign w{p['id']}_0 = {expr(p['id'])};")
        elif p["type"]==CUSTOM:
            i0,i1,i2=[wnr(x) for x in b.ins[p['id']]]
            L.append(f"  full_adder fa{p['id']}(.a({i0}), .b({i1}), .cin({i2}), .sum(w{p['id']}_0), .cout(w{p['id']}_1));")
    # register next-state = their gate expression
    for i in range(W): L.append(f"  assign acc_n[{i}] = {expr(ACC[i][0])};")
    for i in range(W): L.append(f"  assign pc_n[{i}]  = {expr(PC[i][0])};")
    L.append(f"  assign carry_n = {expr(CARRY[0])};")
    # zero output = NOR of acc_n
    L.append("  assign zero = ~(|acc_n);")
    L.append("  always @(posedge clk) begin acc_r<=acc_n; pc_r<=pc_n; carry_r<=carry_n; end")
    L.append("endmodule")
    fa="module full_adder(input a,input b,input cin,output sum,output cout);\n  wire axb=a^b; assign sum=axb^cin; assign cout=(a&b)|(cin&axb);\nendmodule\n"
    open("perf/verilator/cpu8.v","w").write(fa+"\n"+"\n".join(L)+"\n")
    print("wrote perf/verilator/cpu8.v")

if __name__=="__main__" and len(sys.argv)>1 and sys.argv[1]=="verilog":
    emit_verilog()
