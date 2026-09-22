# Half Adder - Half adder

Adds two 1-bit numbers A and B, producing a sum and a carry. `Sum = A XOR B`, `Carry = A AND B`. The simplest arithmetic building block.

**Interface:** 2 inputs (A, B) / 2 outputs (Sum, Carry).

**Validation:** checked exhaustively (all 4 input combinations) against a golden model in `make test` on the interpreted
and both native engines. Compiled into `parts/` by `make fixtures`.

## Reference

Standard textbook digital-logic circuit (a real-world design, not a specific chip).
Background: <https://en.wikipedia.org/wiki/Adder_(electronics)#Half_adder>.
