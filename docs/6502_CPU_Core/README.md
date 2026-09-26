# 6502 CPU Core

A working fetch/decode/execute core for the accumulator instructions. Driven by a clock, it reads
opcodes from the data bus, runs each one, and commits the result to the accumulator and flags -
stepping through a program on its own. It joins the self-timed sequencer to the accumulator
datapath: the sequencer decides timing and holds the opcode, and the datapath performs the
operation, committed on the cycle the sequencer marks as the instruction's last.

## Interface

Inputs (10):
- DB0..DB7 - the data bus (opcode on the fetch cycle, operand on the execute cycle)
- RST      - reset the program counter and timing
- CLK      - the clock

Outputs (41):
- PC0..PC15 - the program counter
- A0..A7    - the accumulator
- N, Z, C, V - the condition flags
- IR0..IR7  - the opcode currently running
- T0, T1, T2 - the cycle within the instruction
- FETCH, DONE - the fetch and last-cycle strobes

## Behaviour

On the fetch cycle the sequencer captures the opcode and advances the program counter. It then
runs the instruction for the number of cycles that opcode needs; on the last cycle it raises
DONE, which both restarts the fetch and enables the datapath's write. So the datapath computes
every cycle but commits only once, on the last cycle, using the held opcode and the operand then
on the bus. This covers the accumulator group that runs in two cycles - the immediate ALU
operations (ORA, AND, EOR, ADC, CMP, SBC with an immediate operand) and the accumulator shifts
and rotates (ASL, ROL, LSR, ROR) - where the operand, when there is one, is the byte after the
opcode.

For example, the opcode `09` with the following byte `55` computes A = A OR $55; the opcode `0A`
shifts A left. A reset clears the program counter and timing to start at address zero; the
accumulator and flags carry across a reset, as on the real device.

## Construction

- Sequencer: the self-timed fetch/execute timing - cycle counter, instruction register, program
  counter and last-cycle decode - producing the opcode, the timing and the DONE strobe.
- Accumulator datapath: the ALU-and-shift accumulator stage, its opcode taken from the
  instruction register, its operand from the data bus, and its write enable driven by DONE, so it
  commits exactly when the instruction finishes.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
