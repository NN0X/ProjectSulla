# 6502 Program Fetch

The fetch unit with a program counter, so it walks through memory on its own. The program
counter supplies the address to read; on each fetch the byte on the data bus is captured into
the instruction register and the counter advances to the next address. Reset clears the counter
and the timing to begin at address zero.

## Interface

Inputs (11):
- DB0..DB7 - the data bus (the byte read from memory)
- RST      - reset: clear the program counter and timing
- DONE     - the current cycle is the instruction's last
- CLK      - the clock

Outputs (28):
- PC0..PC15 - the program counter (the address to read)
- IR0..IR7  - the instruction register (the opcode currently running)
- T0, T1, T2 - the cycle number within the instruction
- FETCH      - high on the fetch cycle

## Behaviour

The program counter holds the address of the byte being fetched. On the fetch cycle the
instruction register loads the data bus and the counter increments, so the next fetch reads the
following address; during an instruction's later cycles the counter holds. Reset clears the
counter to zero and returns the timing to the fetch cycle.

In this stage one byte is fetched per instruction, so the counter advances one step per
instruction; feeding operand bytes will come with the decode that knows each instruction's
length.

## Construction

- Fetch timing and the instruction register, as in the fetch unit.
- Program counter: the 16-bit counter, cleared by reset, its count enable tied to the fetch
  strobe so it advances exactly on the fetch cycle; its load input is held inactive here.

## Reference

MOS 6502 timing: https://www.masswerk.at/6502/6502_instruction_set.html
