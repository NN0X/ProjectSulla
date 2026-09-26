# 6502 ALU Operand Over the SB Bus

The ALU reading its second operand from the shared internal bus rather than from a dedicated
input. The SB bus can carry a register read out of the register file or data from memory, and
whichever it carries becomes the ALU's operand - so the same ALU instruction works on a register
or on a memory byte with no change to the ALU itself. This is the composition that makes
register-operand arithmetic possible.

## Interface

Inputs (41):
- O0..O7   - the opcode
- Ain0..Ain7 - the accumulator (the ALU's first operand)
- WD0..WD7 - register write data
- WS0, WS1 - register write select
- WE       - register write enable
- RS0, RS1 - register read select (which register the bus carries in register mode)
- DIN0..DIN7 - data from memory
- SRC0, SRC1 - the bus source select
- Cin      - the ALU carry in
- CLK      - the clock

Outputs (12):
- Aout0..Aout7 - the ALU result
- N, Z, C, V   - the condition flags

## Behaviour

The SB bus selects its source (SRC = 00 register, 01 memory), and that byte is the ALU's second
operand. The ALU then applies the opcode to the accumulator and the bus operand and produces the
result and flags exactly as it does for an immediate operand:

    Aout = Ain OP (bus operand),   with N, Z always and C, V for the arithmetic operations

Because the operand is whatever the bus carries, a register-operand instruction and a
memory-operand instruction give the same result for the same operand value - the ALU cannot tell
where the operand came from.

## Construction

- SB bus: the register file plus the source-select tri-state banks, producing the shared bus (the
  ALU-result source of the bus is unused here and tied low).
- ALU execute: the accumulator ALU datapath, its M operand wired to the SB bus instead of a
  dedicated operand input.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
