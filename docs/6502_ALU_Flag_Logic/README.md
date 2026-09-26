# 6502 ALU Flag Logic

Computes the four data-dependent 6502 condition flags from an ALU result. It is the
counterpart to the P status register: the decoders decide which flags an instruction
updates, this block produces the values, and the P register stores them.

## Interface

Inputs (12):
- R0..R7 - the ALU result byte
- A7     - sign bit of the first operand
- B7     - sign bit of the second operand
- Cout   - carry out of the ALU (active high: 1 = carry/no-borrow)
- ISSUB  - 1 for a subtraction (SBC/CMP), 0 for an addition (ADC)

Outputs (4):
- N - negative: result bit 7
- Z - zero: result is 0x00
- C - carry: the ALU carry out
- V - signed overflow

## Behaviour

    N = R7
    Z = 1 when R = 0x00, else 0
    C = Cout
    V (add) = operands share a sign and the result's sign differs
    V (sub) = operands differ in sign and the result's sign differs from the first operand

Overflow is derived from the operand and result sign bits rather than the internal
carry into bit 7 (which the ALU does not expose); the ISSUB input selects the add or
subtract form.

## Construction

Gate level. N and C pass through from the result's top bit and the carry line. Z is a
NOR across all eight result bits. V forms the add and subtract overflow terms from the
sign bits and selects between them with ISSUB.

## Reference

6502 status flags: https://www.masswerk.at/6502/6502_instruction_set.html
