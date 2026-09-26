# 6502 Operand Sequencing

Runs a two-operand ALU operation over several cycles on a single shared bus. Because the register
file has one read port and the bus carries one value at a time, the two operands are placed on the
bus on successive cycles and captured into the ALU's input latches; the ALU then operates on the
two latched values. This is the timing that lets a register- or memory-operand instruction execute
with one bus and one read port.

## Interface

Inputs (29):
- T0, T1, T2 - the cycle number within the instruction
- Areg0..Areg7 - the accumulator value (placed on the bus on its cycle)
- Mval0..Mval7 - the operand value (placed on the bus on its cycle)
- O0..O7 - the opcode
- Cin   - the ALU carry in
- CLK   - the clock

Outputs (28):
- Aout0..Aout7 - the ALU result
- N, Z, C, V   - the condition flags
- AI0..AI7     - the captured first operand
- BI0..BI7     - the captured second operand

## Behaviour

The cycle number drives the operand strobes:

    T = 1  load AI: the accumulator is driven onto the bus and captured into AI
    T = 2  load BI: the operand is driven onto the bus and captured into BI
    other  no load: the latches hold and the bus floats

Each operand drives the shared bus only on its own cycle, so the two never contend; after both are
latched the ALU result is AI OP BI. The latches hold across the compute cycle and across operations,
so once loaded an operand stays until the next load.

## Construction

- Strobe decode: the cycle number becomes the two load strobes (AI on cycle 1, BI on cycle 2).
- Shared bus: the accumulator and the operand each gate onto one shared bus through tri-state
  buffers, enabled by the AI and BI load strobes respectively, so the operand for the current cycle
  is what the bus carries.
- ALU input latches: the AI/BI datapath captures the bus into AI or BI on its load strobe and the
  ALU computes over the two latches.

## Reference

6502 internal architecture: https://www.nesdev.org/wiki/Visual6502wiki/6502_datapath
