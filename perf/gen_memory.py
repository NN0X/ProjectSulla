import json, sys

SOURCE, OUTPUT, CUSTOM = 7, 8, 9

def ram_layout(sync, A, W):
    parts, conns = [], []
    nid = 100
    def add(t, ni, no, x, y, lab=""):
        nonlocal nid
        i = nid; nid += 1
        parts.append({"id": i, "type": t, "label": lab, "x": float(x), "y": float(y), "numInputs": ni, "numOutputs": no})
        return i
    we = add(SOURCE, 0, 1, 0, 0)
    addr = [add(SOURCE, 0, 1, 0, 1 + k) for k in range(A)]
    din = [add(SOURCE, 0, 1, 0, 1 + A + k) for k in range(W)]
    mode = "SYNC" if sync else "ASYNC"
    ram = add(CUSTOM, 1 + A + W, W, 400, 0, f"RAM_{mode}_{A}_{W}")
    dout = [add(OUTPUT, 1, 0, 800, k) for k in range(W)]
    def w(f, t): conns.append({"from": {"id": f[0], "pin": f[1]}, "to": {"id": t[0], "pin": t[1]}})
    w((we, 0), (ram, 0))
    for k in range(A): w((addr[k], 0), (ram, 1 + k))
    for k in range(W): w((din[k], 0), (ram, 1 + A + k))
    for p in range(W): w((ram, p), (dout[p], 0))
    return {"parts": parts, "connections": conns}

def emit_verilog(sync, A, W, name):
    L = []
    L.append(f"module {name}(input clk, input we, input [{A-1}:0] addr, input [{W-1}:0] din, output reg [{W-1}:0] dout);")
    L.append(f"  reg [{W-1}:0] mem [0:{(1<<A)-1}];")
    if sync:
        L.append("  always @(posedge clk) begin")
        L.append("    dout <= mem[addr];")
        L.append("    if (we) mem[addr] <= din;")
        L.append("  end")
    else:
        L.append("  always @(posedge clk) if (we) mem[addr] <= din;")
        L.append("  always @(*) dout = mem[addr];")
    L.append("endmodule")
    return "\n".join(L) + "\n"

if __name__ == "__main__":
    A = int(sys.argv[1]) if len(sys.argv) > 1 else 8
    W = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    import os, shutil
    for sync, nm in ((False, "ram_async"), (True, "ram_sync")):
        json.dump(ram_layout(sync, A, W), open(f"perf/layouts/{nm}.json", "w"), indent=2)
        open(f"perf/verilator/{nm}.v", "w").write(emit_verilog(sync, A, W, nm))
        print(f"wrote perf/layouts/{nm}.json  (A={A} W={W}, {1<<A} words)")
    if os.path.isdir("tests/layouts"):
        for nm in ("ram_async", "ram_sync"):
            shutil.copyfile(f"perf/layouts/{nm}.json", f"tests/layouts/{nm}.json")
        print("copied ram_async.json, ram_sync.json -> tests/layouts/")
