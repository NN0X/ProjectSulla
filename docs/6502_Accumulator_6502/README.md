# 6502 Accumulator Datapath (faithful)

The self-updating accumulator built the way the real MOS 6502 builds it: two internal buses, two
ALU input registers loaded at once, and a dedicated result register that holds the ALU output
before it is written back to the accumulator. This is the structure that lets the accumulator
read itself, compute, and store the result each instruction without the compute racing the
write-back.

## Interface

Inputs (19):
- RST   - reset the timing to the first cycle
- DIN0..DIN7 - the memory operand (arrives on the data bus DB)
- O0..O7 - the opcode
- Cin   - the ALU carry in
- CLK   - the clock

Outputs (16):
- A0..A7 - the accumulator
- T0, T1, T2 - the cycle number
- DONE   - high on the write-back cycle (the instruction's last)
- N, Z, C, V - the ALU condition flags

## Behaviour

Each instruction runs a four-cycle micro-sequence over the two buses:

    cycle 0  fetch
    cycle 1  the accumulator drives the special bus SB into the AI input register, and the memory
             operand drives the data bus DB into the BI input register - both loaded at once, one
             from each bus
    cycle 2  the ALU computes AI OP BI and the result is captured into the ADD result register
    cycle 3  the ADD register drives SB back into the accumulator; DONE is raised and the counter
             returns to cycle 0

Because AI and BI come from two separate buses they load simultaneously with no time-multiplexing,
and because the accumulator loads from the ADD register - a registered, stable value - rather than
directly from the live ALU output, the write-back never samples a still-settling result. The
accumulator therefore updates correctly every instruction, A = A OP operand.

## Construction

- Two buses: SB (special) carries the accumulator and, on write-back, the ADD register; DB (data)
  carries the memory operand. Each source gates onto its bus through tri-state buffers.
- AI, BI: the two ALU input registers, AI loaded from SB and BI from DB on the input cycle.
- ALU: the accumulator ALU datapath over AI and BI.
- ADD: the result register, capturing the ALU output on the compute cycle.
- A: the accumulator, loading from SB (driven by ADD) on the write-back cycle.
- Cycle counter: the T-state timing, its DONE input the write-back strobe, so the instruction is
  four cycles and self-timed.

## Reference

6502 internal datapath: https://www.nesdev.org/wiki/Visual6502wiki/6502_datapath
