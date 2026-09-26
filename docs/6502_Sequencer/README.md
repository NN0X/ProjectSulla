# 6502 Sequencer

The self-timed fetch/execute engine. Driven only by a clock, it fetches an opcode, runs it for
the number of cycles that opcode needs, and fetches the next - stepping through a program on its
own. It closes the loop between the timing counter, the instruction register, the program
counter, and the last-cycle decode: the opcode being run and the current cycle feed the DONE
decode, whose output tells the counter when to finish and begin the next fetch.

## Interface

Inputs (10):
- DB0..DB7 - the data bus (the byte read from memory)
- RST      - reset: clear the program counter and timing
- CLK      - the clock

Outputs (29):
- PC0..PC15 - the program counter (the address to read)
- IR0..IR7  - the instruction register (the opcode running)
- T0, T1, T2 - the cycle number within the instruction
- FETCH      - high on the fetch cycle
- DONE       - high on the instruction's last cycle

## Behaviour

On the fetch cycle the instruction register captures the bus and the program counter advances.
The DONE decode reads the held opcode and the current cycle and raises DONE on the opcode's last
cycle, which returns the counter to the fetch cycle so the next opcode is read. A two-cycle
instruction runs cycles 0 and 1; a four-cycle instruction runs 0 through 3; and so on, each
determined by the opcode with no external cycle input. Reset clears the counter and the program
counter to begin at address zero.

The instruction register only loads on the fetch cycle, and the last cycle is never cycle zero,
so the opcode a running instruction sees is stable for the whole instruction.

## Construction

- Cycle counter: the T-state timing, whose DONE input is driven from the decode below.
- Instruction register: loads the bus on the fetch cycle, holds otherwise.
- Program counter: cleared on reset, advanced on the fetch cycle.
- DONE decode: the opcode in the instruction register and the counter's cycle number produce the
  last-cycle signal wired back into the counter.

This wiring makes a cycle: the counter's cycle number feeds the decode and the decode feeds the
counter's next state, closed each clock through the counter's own register.

## Reference

MOS 6502 timing: https://www.masswerk.at/6502/6502_instruction_set.html
