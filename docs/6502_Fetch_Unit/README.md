# 6502 Fetch Unit

The front of the control unit: it times instructions and holds the opcode being run. On the
fetch cycle it latches the byte on the data bus into the instruction register; through the rest
of the instruction that register holds the opcode steady for the decode logic, while the cycle
counter tracks how far into the instruction the machine is.

## Interface

Inputs (11):
- DB0..DB7 - the data bus (the byte being read)
- RST      - reset the timing to the fetch cycle
- DONE     - the current cycle is the instruction's last
- CLK      - the clock

Outputs (12):
- IR0..IR7  - the instruction register (the opcode currently running)
- T0, T1, T2 - the cycle number within the instruction
- FETCH      - high on the fetch cycle

## Behaviour

The cycle counter advances once per clock and returns to the fetch cycle on RST or DONE. On the
fetch cycle (and only then) the instruction register loads the data bus, so the opcode read
during fetch is captured and then held while the counter walks through the instruction's later
cycles. A reset or a DONE begins the next instruction's fetch on the following clock.

## Construction

- Cycle counter: the instruction T-state timing, producing the cycle number and the FETCH strobe.
- Instruction register: a 74377 whose data is the bus and whose load enable is FETCH, so it
  captures on the fetch cycle and holds otherwise.

## Reference

MOS 6502 timing: https://www.masswerk.at/6502/6502_instruction_set.html
