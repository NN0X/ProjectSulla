# 6502 Branch Condition

Decides whether a conditional branch is taken. The eight branch instructions each test one
condition flag against a wanted value; this unit reads the opcode and the current flags and
raises a single TAKEN line when the branch should be followed.

    opcode   branch   taken when
    0x10     BPL      N = 0
    0x30     BMI      N = 1
    0x50     BVC      V = 0
    0x70     BVS      V = 1
    0x90     BCC      C = 0
    0xB0     BCS      C = 1
    0xD0     BNE      Z = 0
    0xF0     BEQ      Z = 1

## Interface

Inputs (12):
- O0..O7   - the opcode
- N, Z, C, V - the current condition flags

Outputs (1):
- TAKEN - 1 when the opcode is a branch and its condition holds

## Behaviour

The branch opcodes share the low five bits 1 0000, so the group is recognised by those bits.
The top three bits select the flag and the wanted value: opcode bits 7 and 6 choose the flag
(00 = N, 01 = V, 10 = C, 11 = Z) and bit 5 is the value that flag must equal for the branch to
be taken.

    TAKEN = (opcode is a branch) AND (selected flag = opcode bit 5)

For any opcode that is not a branch, TAKEN is 0.

## Construction

Gate level. A five-input AND on the low opcode bits forms the branch-group detect. A pair of
two-way multiplexers selects one flag from the four using opcode bits 7 and 6. That flag is
compared to bit 5 with an XNOR, and the result is ANDed with the group detect.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
