# 6502 ALU Execute Datapath

The execute stage for the accumulator ALU instructions: given an opcode and the two
operands, it produces the new accumulator value and the resulting condition flags. It
composes three already-validated blocks - the ALU control decoder, the 8-bit ALU (a
74181 pair) and the ALU flag logic - into one combinational datapath, so a whole
instruction's arithmetic runs in a single pass.

It covers the full accumulator group for cc = 01: the logic operations ORA, AND and EOR,
the add ADC, and the subtract/compare SBC and CMP.

## Interface

Inputs (25):
- O0..O7   - the opcode
- Ain0..7  - the current accumulator
- M0..M7   - the second operand (memory / immediate)
- Cin      - the current carry flag (used by ADC and SBC)

Outputs (12):
- Aout0..7 - the ALU result (the new accumulator)
- N, Z, C, V - the computed condition flags

## Behaviour

The decoder turns the opcode into the ALU's function select and mode; the ALU applies it
to Ain and M; the flag logic reads the result to produce N, Z, C and V:

    ORA:  Aout = Ain OR  M
    AND:  Aout = Ain AND M
    EOR:  Aout = Ain XOR M
    ADC:  Aout = Ain + M + Cin,        C = carry out
    SBC:  Aout = Ain - M - (1 - Cin),  C = 1 on no borrow
    CMP:  Aout = Ain - M,              C = 1 on no borrow

For the logic operations N and Z are meaningful (C and V are computed but not used by
those instructions); for ADC, SBC and CMP all four flags apply, except that CMP's caller
holds the carry in high and does not commit V. Which flags a given instruction actually
commits to the P register, and whether it writes the accumulator at all, is decided
elsewhere, by the decode/control logic.

## Construction

- ALU control decoder: opcode -> function select S0-3 and mode.
- 8-bit ALU: the two operands under that control -> result and carry.
- Flag logic: result plus operand sign bits and carry -> N, Z, C, V.
- Glue: the ALU carry-in is mode OR NOT-Cin, which is a don't-care for the logic ops and
  the carry flag for ADC and SBC (the same form serves add and subtract, since a 74181
  subtract borrows when the carry-in is low). The flag logic's subtract-select is driven
  high for CMP and SBC (opcode group cc = 01 with the top two aaa bits set) so its overflow
  term uses the subtract form.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
