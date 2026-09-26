# 6502 ALU Input Latches

The two registers that hold the ALU's operands. The register file has a single read port, so a
register-operand instruction cannot read both the accumulator and the operand register at once;
instead each operand is read onto the internal bus on its own cycle and captured into an input
latch, and the ALU then operates on the two latched values. This is the AI/BI input-register
structure the 6502 uses.

## Interface

Inputs (20):
- SBIN0..SBIN7 - the internal bus feeding the latches
- LDAI - load the AI latch from the bus this clock
- LDBI - load the BI latch from the bus this clock
- O0..O7 - the opcode
- Cin  - the ALU carry in
- CLK  - the clock

Outputs (28):
- Aout0..Aout7 - the ALU result
- N, Z, C, V   - the condition flags
- AI0..AI7     - the AI latch (the first operand)
- BI0..BI7     - the BI latch (the second operand)

## Behaviour

Each latch captures the bus on a clock where its load line is high and holds otherwise. So a
sequence loads AI on one cycle and BI on the next, each from whatever the bus carries, and both
then hold while the ALU computes:

    Aout = AI OP BI

A latch keeps its value across operations until it is loaded again - loading AI does not disturb
BI, and the held operands survive the compute cycle - so the control unit can leave one operand
in place and reload only the other.

## Construction

- AI latch: a 74377 whose data is the bus and whose load enable is LDAI.
- BI latch: a 74377 whose data is the bus and whose load enable is LDBI.
- ALU execute: the accumulator ALU datapath, its first operand wired to AI and its second to BI.

## Reference

6502 internal architecture: https://www.nesdev.org/wiki/Visual6502wiki/6502_datapath
