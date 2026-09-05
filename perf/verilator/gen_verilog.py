import sys
# Gate-level ripple-carry adders matching Sulla benchmark I/O widths.
# full_adder: a,b,cin -> sum,cout   (3 in, 2 out)
# adderN    : a[N-1:0],b[N-1:0] -> s[N-1:0],cout  (2N in, N+1 out), cin=0

FA = """module full_adder(input a, input b, input cin, output sum, output cout);
  wire axb = a ^ b;
  assign sum  = axb ^ cin;
  assign cout = (a & b) | (cin & axb);
endmodule
"""

def adder(n):
    lines = [f"module adder{n}(input [{n-1}:0] a, input [{n-1}:0] b, output [{n-1}:0] s, output cout);"]
    lines.append(f"  wire [{n}:0] c;")
    lines.append("  assign c[0] = 1'b0;")
    for i in range(n):
        lines.append(f"  full_adder fa{i}(.a(a[{i}]), .b(b[{i}]), .cin(c[{i}]), .sum(s[{i}]), .cout(c[{i+1}]));")
    lines.append(f"  assign cout = c[{n}];")
    lines.append("endmodule")
    return "\n".join(lines) + "\n"

name = sys.argv[1]
with open(f"{name}.v", "w") as f:
    f.write(FA)
    if name == "full_adder":
        pass  # top is full_adder itself
    else:
        n = int(name.replace("adder",""))
        f.write("\n" + adder(n))
