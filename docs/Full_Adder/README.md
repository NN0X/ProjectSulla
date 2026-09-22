# Full Adder - Full adder

Adds two 1-bit numbers plus a carry-in: `Sum = A XOR B XOR Cin`, `Cout = (A AND B) OR (Cin AND (A XOR B))`. Chaining full adders builds ripple-carry adders (see the 74283).

**Interface:** 3 inputs (A, B, Cin) / 2 outputs (Sum, Cout).

**Validation:** checked exhaustively (all 8 input combinations) against a golden model in `make test` on the interpreted
and both native engines. Compiled into `parts/` by `make fixtures`.

## Reference

Standard textbook digital-logic circuit (a real-world design, not a specific chip).
Background: <https://en.wikipedia.org/wiki/Adder_(electronics)#Full_adder>.
