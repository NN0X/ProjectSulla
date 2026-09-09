import json, sys, os, shutil

SOURCE, OUTPUT, CUSTOM = 7, 8, 9

def arith_layout(is_mul, W):
    parts, conns = [], []
    nid = 100
    def add(t, ni, no, x, y, lab=""):
        nonlocal nid
        i = nid; nid += 1
        parts.append({"id": i, "type": t, "label": lab, "x": float(x), "y": float(y), "numInputs": ni, "numOutputs": no})
        return i
    A = [add(SOURCE, 0, 1, 0, k) for k in range(W)]
    B = [add(SOURCE, 0, 1, 0, W + k) for k in range(W)]
    nout = 2 * W if is_mul else W + 1
    lab = ("MUL_" if is_mul else "ADD_") + str(W)
    node = add(CUSTOM, 2 * W, nout, 400, 0, lab)
    outs = [add(OUTPUT, 1, 0, 800, p) for p in range(nout)]
    def w(f, t): conns.append({"from": {"id": f[0], "pin": f[1]}, "to": {"id": t[0], "pin": t[1]}})
    for k in range(W): w((A[k], 0), (node, k))
    for k in range(W): w((B[k], 0), (node, W + k))
    for p in range(nout): w((node, p), (outs[p], 0))
    return {"parts": parts, "connections": conns}

def emit_verilog(is_mul, W, name):
    if is_mul:
        return (f"module {name}(input [{W-1}:0] a, input [{W-1}:0] b, output [{2*W-1}:0] p);\n"
                f"  assign p = a * b;\n"
                f"endmodule\n")
    return (f"module {name}(input [{W-1}:0] a, input [{W-1}:0] b, output [{W-1}:0] s, output cout);\n"
            f"  assign {{cout, s}} = a + b;\n"
            f"endmodule\n")

CIRCUITS = [("wadd8", False, 8), ("wmul8", True, 8), ("wmul16", True, 16)]

if __name__ == "__main__":
    for name, is_mul, W in CIRCUITS:
        json.dump(arith_layout(is_mul, W), open(f"perf/layouts/{name}.json", "w"), indent=2)
        open(f"perf/verilator/{name}.v", "w").write(emit_verilog(is_mul, W, name))
        print(f"wrote perf/layouts/{name}.json  ({'MUL' if is_mul else 'ADD'}_{W})")
    if os.path.isdir("tests/layouts"):
        for name, _, _ in CIRCUITS:
            shutil.copyfile(f"perf/layouts/{name}.json", f"tests/layouts/{name}.json")
        print("copied word-arith layouts -> tests/layouts/")
