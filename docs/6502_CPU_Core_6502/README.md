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
- N, Z, C, V - the condition flags, held in the P status register and updated per instruction by
  the flags that instruction affects; C is fed back as the ALU carry-in
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

The condition flags live in the P status register, which has a per-bit load enable so each
instruction updates only the flags it affects: the logic operations (ORA, AND, EOR) update N and
Z, the arithmetic operations (ADC, SBC) update N, Z, C and V, and the compare (CMP) updates N, Z
and C. The ALU's carry-in is taken from the register's C bit, so the carry is carried across
instructions - and because a logic operation does not enable the C or V load, a logic instruction
between two arithmetic ones leaves the carry and overflow untouched. This covers the memory-operand
accumulator group ORA, AND, EOR, ADC, SBC, CMP and LDA (operand the byte after the opcode) plus
the accumulator shifts and rotates ASL, ROL, LSR and ROR.

CMP is the subtract with two differences from SBC: its carry-in is forced high (a full compare,
not a borrow chain), and it does not write the accumulator - only the flags are updated, so it
reports how A compares with the operand (C set when A is greater or equal) while leaving A alone.

LDA loads the accumulator from the operand: A takes the memory byte directly (it drives the
special bus on write-back instead of the ALU result), and N and Z are taken from that byte while C
and V are left unchanged.

The shift and rotate instructions ASL, ROL, LSR and ROR work on the accumulator: a one-place
shifter takes A, its direction from the opcode's direction bit and its fill (a shifted-in zero or
the carry) from the rotate bit and the carry flag, and its result is written back to A the same way
the ALU result is. They update N and Z from the result and C from the bit shifted out.

For example, `61 50` (ADC) adds and sets C and V from the result; a following `41 FF` (EOR)
updates N and Z but leaves C and V as the ADC left them; a `C1 9E` (CMP) then sets N, Z and C from
A minus the operand without disturbing A or V.

A reset clears the program counter and timing to start at address zero; the accumulator and carry
carry across a reset, as on the real device.

## Construction

- Program fetch: the shared cycle counter, the instruction register (latched on the fetch cycle)
  and the program counter (advanced on the fetch cycle) - one counter for the whole instruction,
  its DONE input the write-back strobe, so an instruction is four cycles.
- Phase decode: T0 fetch, T1 load AI/BI, T2 load ADD, T3 write back - a few gates off the counter.
- Datapath: SB and DB buses, the AI and BI input registers, the accumulator ALU, a one-place
  shifter, the ADD result register and the accumulator A - the faithful accumulator, its opcode
  taken from IR, its operand from DB and its carry-in from the P register. The ADD register takes
  the shifter's output on a shift and the ALU's output otherwise; on write-back the special bus is
  driven by the ADD register for the ALU/shift/compare group or, for LDA, by the operand register -
  a bus source select in place of a discrete multiplexer. N is the result's bit 7 (or the operand's
  for LDA), Z is the result being zero (or the operand for LDA), and C is the shifter's carry-out on
  a shift or the ALU carry otherwise.
- P status register: the condition flags, taken from the ALU flag outputs, with per-bit load
  enables raised on the write-back cycle - N and Z for the whole group, C for the arithmetic
  operations and CMP, V for the arithmetic operations only - and its C bit fed back as the ALU
  carry-in. For CMP the carry-in is forced high (an OR of the compare-decode into the carry line)
  and the accumulator's load enable is suppressed, so the compare updates flags without writing A.

## Reference

6502 internal datapath: https://www.nesdev.org/wiki/Visual6502wiki/6502_datapath
