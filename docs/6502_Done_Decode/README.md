# 6502 DONE Decode

Signals the last cycle of an instruction. Given the opcode and the current cycle number, it
raises DONE on the cycle where the instruction finishes, which the cycle counter uses to return
to the fetch cycle and start the next instruction. This is what lets instructions of different
lengths run the right number of cycles instead of a fixed count.

## Interface

Inputs (11):
- O0..O7   - the opcode
- T0, T1, T2 - the current cycle number

Outputs (1):
- DONE - high on the instruction's last cycle

## Behaviour

The opcode selects a cycle count; DONE is high when the current cycle number equals that count
minus one (the last cycle). For the cc = 01 accumulator ALU group the count comes from the
addressing-mode field:

    immediate           2 cycles
    zero page           3
    absolute            4
    zero page,X         4
    absolute,X / ,Y     4
    (zero page),Y       5
    (zero page,X)       6

Every other opcode uses a two-cycle default, which is correct for the implied, accumulator and
immediate instructions that run without a memory operand. Longer non-cc = 01 instructions will
get their counts as those addressing modes are decoded.

## Construction

A three-to-eight decode of the addressing-mode field drives, through OR terms, the three bits of
the last-cycle number for the cc = 01 group, with a two-cycle default selected for other opcodes.
That number is compared to the current cycle with a bit-wise equality (three XNORs ANDed
together) to produce DONE.

## Reference

MOS 6502 timing: https://www.masswerk.at/6502/6502_instruction_set.html
