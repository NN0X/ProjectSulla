# 74181 - 4-bit arithmetic logic unit

Real gate-level implementation of the 74181, the first complete ALU on a single chip
(Texas Instruments, 1970) and the arithmetic core of many 1970s minicomputers. It
performs 16 arithmetic and 16 logic operations on two 4-bit operands, selected by the
four function-select inputs S0-S3 and the mode input M (M=1 logic, M=0 arithmetic),
with carry lookahead.

Built from the documented carry-lookahead structure: for each bit,
`P = A OR (B AND S0) OR (NOT B AND S1)` and `G = A AND ((B AND S3) OR (NOT B AND S2))`;
the result bit is `F = P XOR G XOR C`, with the carry chain `C(i+1) = G OR (P AND C)`
and M forcing the carries so logic operations drop the carry. Operands are active-high;
the carry Cn and carry-out Cn+4 are active-low (Cn HIGH = no carry-in), matching the
datasheet.

This is the building block of the 6502's ALU: two 74181s cascaded give the 8-bit ALU.

**Interface:** 14 inputs (A0-A3, B0-B3, S0-S3, M, Cn) / 6 outputs (F0-F3, Cn+4, A=B).

## Reference

74181 (Texas Instruments SN74LS181): <https://en.wikipedia.org/wiki/74181>
