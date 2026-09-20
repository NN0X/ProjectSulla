# 74283 - 4-bit binary full adder

Real gate-level implementation of the 74283 MSI TTL part, built entirely from
2-input primitive gates.

Adds two 4-bit binary numbers A (A1..A4) and B (B1..B4) with a carry-in C0, producing a 4-bit sum S1..S4 and a carry-out C4. Internally each bit is a full adder: `P = A xor B`, `S = P xor Cin`, and carry `Cout = (A and B) or (P and Cin)`, chained C0->C4.

**Interface:** 9 inputs (A1..A4, B1..B4, C0) / 5 outputs (S1..S4, C4).

**Validation:** Exhaustively validated: for all 512 input combinations, `{S1..S4,C4}` equals the 5-bit value `A + B + C0` (A1 and B1 are the least-significant bits). It is compiled into `parts/` by `make fixtures` and checked
in `make test` on both the interpreted and native engines.

## Reference

Texas Instruments 74283 datasheet (SN7474283). The datasheet PDF is copyrighted by
Texas Instruments and is **not redistributed here**; see the official document at
<https://www.ti.com/lit/gpn/sn7474283>. The function and pin behaviour summarised
above are factual and taken from that specification.
