# 74283 - 4-bit binary full adder

Real gate-level implementation of the 74283 MSI TTL part, built entirely from
2-input primitive gates.

Adds two 4-bit binary numbers A (A1..A4) and B (B1..B4) with a carry-in C0, producing a 4-bit sum S1..S4 and a carry-out C4. Internally each bit is a full adder: `P = A xor B`, `S = P xor Cin`, and carry `Cout = (A and B) or (P and Cin)`, chained C0->C4.

**Interface:** 9 inputs (A1..A4, B1..B4, C0) / 5 outputs (S1..S4, C4).

## Reference

Texas Instruments SN74HC283 datasheet: <https://www.ti.com/lit/ds/symlink/sn74hc283.pdf>
