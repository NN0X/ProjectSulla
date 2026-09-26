# 6502 Compare and BIT Flags

The flag update for the cc = 00 test instructions: the compares CPX and CPY, and BIT. None of
them write a register - they only update condition flags - so this unit takes the current flags
in and produces the updated flags out, leaving untouched flags unchanged.

    CPX / CPY   flags from R - M   (R is the X or Y register)
    BIT         flags from A AND M and from M itself

## Interface

Inputs (28):
- O0..O7 - the opcode
- R0..R7 - the register operand (X for CPX, Y for CPY, A for BIT)
- M0..M7 - the memory operand
- Nin, Zin, Cin, Vin - the current condition flags

Outputs (4):
- N, Z, C, V - the updated condition flags

## Behaviour

A compare subtracts without borrow and sets the flags from the result, like CMP but reading an
index register and storing nothing:

    CPX / CPY:  N = bit 7 of (R - M),  Z = 1 when R = M,  C = 1 when R >= M;  V unchanged

BIT tests the accumulator against memory but takes two of its flags straight from the memory
operand:

    BIT:  Z = 1 when (A AND M) = 0,  N = M7,  V = M6;  C unchanged

Any opcode outside this group passes all four flags through unchanged.

## Construction

- The subtract R - M is the 8-bit ALU (the 74181 pair) held in subtract mode with no borrow in;
  its result gives N and the zero-detect gives Z, and its carry out gives C.
- BIT's zero term is a byte-wide AND of R and M followed by a zero-detect; its N and V are just
  bits 7 and 6 of M.
- A small cc = 00 decode forms the compare detect (top two aaa bits set) and the BIT detect, and
  a per-flag multiplexer chooses, for each output, the compare value, the BIT value, or the
  incoming flag.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
