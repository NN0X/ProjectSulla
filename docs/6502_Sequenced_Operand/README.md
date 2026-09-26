# 6502 Self-Timed Operand Execution

A two-operand ALU instruction running on a clock alone. The cycle counter walks the instruction's
cycles, the operand strobes load the two operands into the ALU input latches on successive cycles,
and the instruction ends itself when the counter reaches the compute cycle - no external timing.
This is the operand sequencing of the previous unit driven by the machine's own cycle counter
instead of hand-fed cycle numbers.

## Interface

Inputs (27):
- RST   - reset the timing to the first cycle
- Areg0..Areg7 - the accumulator value
- Mval0..Mval7 - the operand value
- O0..O7 - the opcode
- Cin   - the ALU carry in
- CLK   - the clock

Outputs (32):
- Aout0..Aout7 - the ALU result
- N, Z, C, V   - the condition flags
- T0, T1, T2   - the cycle number
- DONE         - high on the compute cycle (the instruction's last)
- AI0..AI7, BI0..BI7 - the captured operands

## Behaviour

Each clock advances the cycle counter, and the instruction runs a fixed four-cycle micro-sequence:

    cycle 0  fetch
    cycle 1  load AI - the accumulator is captured into the first ALU input latch
    cycle 2  load BI - the operand is captured into the second
    cycle 3  compute - the ALU result AI OP BI is valid, DONE is raised, and the counter
             returns to cycle 0 for the next instruction

DONE is the counter's own terminator: it is high exactly on the compute cycle, feeds back into the
counter to restart it, and so the machine steps from one operand instruction to the next with only
a clock. The result and flags at the compute cycle are AI OP BI over the two latched operands.

## Construction

- Cycle counter: the T-state timing, its DONE input driven by the terminator below.
- Done terminator: a gate that is high when the cycle number is the compute cycle (three), wired
  back into the counter so the instruction is four cycles long.
- Operand sequencing: the strobe-plus-latch datapath, its cycle number taken from the counter.

## Reference

6502 internal architecture: https://www.nesdev.org/wiki/Visual6502wiki/6502_datapath
