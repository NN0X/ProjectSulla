# 6502 ALU Execute Datapath

The execute stage for the accumulator ALU instructions: given an opcode and the two
operands, it produces the new accumulator value and the resulting condition flags. It
composes three already-validated blocks - the ALU control decoder, the 8-bit ALU (a
74181 pair) and the ALU flag logic - into one combinational datapath, so a whole
instruction's arithmetic runs in a single pass.

Version 1 covers the logic and add group: ORA, AND, EOR and ADC. Subtract and compare
(SBC, CMP) follow once borrow/carry-mode control is added.

## Interface

Inputs (25):
- O0..O7   - the opcode
- Ain0..7  - the current accumulator
- M0..M7   - the second operand (memory / immediate)
- Cin      - the current carry flag (used by ADC)

Outputs (12):
- Aout0..7 - the ALU result (the new accumulator)
- N, Z, C, V - the computed condition flags

## Behaviour

The decoder turns the opcode into the ALU's function select and mode; the ALU applies it
to Ain and M; the flag logic reads the result to produce N, Z, C and V:

    ORA:  Aout = Ain OR  M
    AND:  Aout = Ain AND M
    EOR:  Aout = Ain XOR M
    ADC:  Aout = Ain + M + Cin,  C = carry out

For the logic operations N and Z are meaningful (C and V are computed but not used by
those instructions); for ADC all four flags apply. Which flags a given instruction
actually commits to the P register is decided elsewhere, by the decode/control logic.

## Construction

- ALU control decoder: opcode -> function select S0-3 and mode.
- 8-bit ALU: the two operands under that control -> result and carry.
- Flag logic: result plus operand sign bits and carry -> N, Z, C, V.
- Glue: the ALU carry-in is the carry flag for ADC and a don't-care for the logic ops
  (mode OR NOT-Cin); the subtract select is held low in this version.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
