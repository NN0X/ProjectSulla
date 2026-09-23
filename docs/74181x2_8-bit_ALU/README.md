# 74181 x2 - 8-bit arithmetic logic unit

An 8-bit ALU formed from two 74181 4-bit slices with a ripple carry between them (the
low slice's carry-out feeds the high slice's carry-in). It performs the 74181's 16
arithmetic and 16 logic operations on two 8-bit operands, selected by S0-S3 and the
mode input M, and is the arithmetic core of the 6502.

For each bit the 74181 slice computes `P = A OR (B AND S0) OR (NOT B AND S1)`,
`G = A AND ((B AND S3) OR (NOT B AND S2))`, and `F = P XOR G XOR C` with carry
lookahead; the carry-in Cn and carry-out Cn8 are active-low (Cn HIGH = no carry-in).

**Interface:** 22 inputs (A0-A7, B0-B7, S0-S3, M, Cn) / 10 outputs (F0-F7, Cn8, A=B).

## Reference

74181 (Texas Instruments SN74LS181): <https://en.wikipedia.org/wiki/74181>
