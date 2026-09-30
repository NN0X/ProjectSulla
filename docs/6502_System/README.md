# 6502 System (CPU core + program memory)

The first whole computer: the faithful 6502 CPU core wired to a memory holding a real program, so
the core fetches its instructions from memory rather than from an external data-bus input. Clock it
and it runs the program on its own.

## Interface

Inputs (2):
- RST - reset the processor: program counter and timing to zero
- CLK - the clock

Outputs (60):
- PC0..PC15 - the program counter
- A0..A7    - the accumulator
- X0..X7    - the X index register
- Y0..Y7    - the Y index register
- SP0..SP7  - the stack pointer
- N, Z, C, V - the condition flags
- IR0..IR7  - the opcode currently running

There is no external data bus: the operand and opcode bytes come from memory inside the part.

## Behaviour

The core's program counter drives the memory's address, and the memory's data drives the core's
data bus. On every fetch cycle the byte at the current PC is read back as the opcode; on an operand
cycle the next byte is read back as the operand. The core is otherwise unchanged - the same
four-cycle micro-sequence per instruction, the same datapath - so a program laid out in memory as a
stream of opcodes and operands executes byte by byte exactly as it would on the real chip.

The memory here is a ROM, so the program is fixed; the address is the low 8 bits of PC, so the
first 256 bytes are visible. That is enough to run a straight-line program end to end: fetch,
decode, execute, advance, repeat, with each instruction's result visible on A, X, Y, SP and the
flags as it retires.

The baked-in program exercises the whole instruction set the core supports - load immediate, index
loads, transfers, inc/dec, a flag set, add and subtract with carry, a shift and a compare - and the
registers walk through the expected values as it runs, which is what the system test checks cycle by
cycle against an instruction-level model of the same program.

## Construction

- 6502_CPU_Core_6502 - the faithful CPU core, unchanged.
- PROGRAM - a ROM part holding the program bytes; its address is the core's PC low 8 bits and its
  data is wired to the core's data-bus inputs.
- The two are wired PC -> ROM address and ROM data -> DB, with RST and CLK brought out to the core.
  The core's register and flag outputs are brought out as the system's outputs.

This is the memory milestone: the processor now runs a program from memory instead of being fed
instructions from outside. The next steps add a writable memory and the address path for stores, so
the processor can write results back as well as read instructions.
