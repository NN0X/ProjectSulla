# 6502 CPU Core (faithful datapath)

A working fetch/decode/execute core built on the faithful MOS 6502 accumulator datapath - two
internal buses SB and DB, the two ALU input registers AI and BI loaded at once, and the ADD
result register - rather than the single-cycle accumulator of the first core. Driven by a clock,
it reads opcodes and operands from the data bus, runs each accumulator-group instruction as the
real chip's four-phase micro-sequence, and commits the result to the accumulator, with the carry
flag fed back across instructions so arithmetic chains correctly.

## Interface

Inputs (10):
- DB0..DB7 - the data bus: the opcode on the fetch cycle, the memory operand on the load cycle
- RST      - reset the program counter and timing
- CLK      - the clock

Outputs (41):
- PC0..PC15 - the program counter
- A0..A7    - the accumulator
- N, Z, C, V - the condition flags (C is registered and fed back as the ALU carry-in; N, Z, V are
  the live ALU flags pending the full P status register)
- IR0..IR7  - the opcode currently running
- T0, T1, T2 - the cycle within the instruction
- FETCH, DONE - the fetch and last-cycle strobes

## Behaviour

Each instruction runs the four-cycle micro-sequence of the faithful accumulator, timed by one
counter shared with the fetch unit:

    cycle 0  fetch    - the opcode is latched into IR and the program counter advances
    cycle 1  load     - the accumulator drives SB into AI and the memory operand drives DB into
                        BI, both loaded at once, one from each bus
    cycle 2  compute  - the ALU computes AI OP BI and the result is captured into the ADD register
    cycle 3  writeback- the ADD register drives SB back into the accumulator; DONE is raised and
                        the counter returns to cycle 0

Because AI and BI come from two separate buses they load simultaneously, and because the
accumulator loads from the registered ADD result rather than the live ALU output, the write-back
never samples a still-settling value - the accumulator updates correctly every instruction.

The carry flag is held in a one-bit register loaded only on the arithmetic opcodes (ADC and SBC),
so a logic instruction between two arithmetic ones leaves the carry intact, and the ALU's carry-in
is taken from this register - A = A OP operand with the carry carried across instructions, as on
the real device. This covers the accumulator group that reads a memory operand: ORA, AND, EOR,
ADC and SBC, where the operand is the byte after the opcode.

For example, `61 01` computes A = A + 1 + C and updates the carry; a following `21 0C` (AND) leaves
that carry untouched; a later `61 00` then adds it back in.

A reset clears the program counter and timing to start at address zero; the accumulator and carry
carry across a reset, as on the real device.

## Construction

- Program fetch: the shared cycle counter, the instruction register (latched on the fetch cycle)
  and the program counter (advanced on the fetch cycle) - one counter for the whole instruction,
  its DONE input the write-back strobe, so an instruction is four cycles.
- Phase decode: T0 fetch, T1 load AI/BI, T2 load ADD, T3 write back - a few gates off the counter.
- Datapath: SB and DB buses, the AI and BI input registers, the accumulator ALU, the ADD result
  register and the accumulator A - the faithful accumulator, its opcode taken from IR, its operand
  from DB and its carry-in from the carry register.
- Carry register: one bit, loaded on the write-back cycle only for ADC/SBC, feeding the ALU
  carry-in.

## Reference

6502 internal datapath: https://www.nesdev.org/wiki/Visual6502wiki/6502_datapath
