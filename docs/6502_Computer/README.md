# 6502 Computer (CPU + program ROM + data RAM on the unified bus)

The processor's own writes feeding its own reads: the faithful CPU core wired to a program ROM and a
data RAM over the one shared address/data bus, so a program can store a result to memory and read it
back. Where the earlier 6502_System only fetched instructions from a ROM, this is the whole loop -
fetch, execute, store, load - over the real 6502's external pins.

## Interface

Inputs (2):
- RST - reset the processor
- CLK - the clock

Outputs (60):
- PC0..PC15, A0..A7, X0..X7, Y0..Y7, SP0..SP7, N, Z, C, V, IR0..IR7 - the registers and flags, brought
  out for observation.

## Memory map

- 0x00-0x7F - the program ROM (the code).
- 0x80-0xFF - the data RAM (writable).

The split is decoded from address bit 7: the ROM answers when it is low, the RAM when it is high. The
program lives in the low half; a store or a zero-page load to $80-$FF lands in the RAM.

## Behaviour

The core drives one 16-bit address bus AB, one data-out bus DBout and one read/write line RW (the
memory interface built with STA and LDA zero-page). Those are tied to the two memories:

- The address bus low byte feeds both the ROM and the RAM address inputs.
- The data bus is bidirectional, built from tri-state drivers onto a shared bus: the ROM drives it on
  a read from the ROM half (RW high, address bit 7 low), the RAM drives it on a read from the RAM half
  (RW high, bit 7 high), and the core drives it on a write (RW low). Exactly one drives at a time.
- The RAM's write-enable is RW low and address bit 7 high, so a store writes the RAM and never the ROM.
- The shared bus feeds both the core's data-in and the RAM's data-in.

On a fetch or an operand read the address is the program counter (in the ROM half), so the ROM drives
the instruction byte onto the bus. On an STA the core drives the accumulator onto the bus with RW low
and the RAM latches it. On an LDA zero-page the core drives the effective address (in the RAM half),
the RAM drives the stored byte back onto the bus, and it is read into the accumulator.

The baked-in program proves the loop:

    LDA #$42 ; STA $80 ; LDA #$99 ; STA $81 ; LDA #$00 ; LDA $80 ; LDA $81

It stores 0x42 to $80 and 0x99 to $81, clears the accumulator, then loads both back - and the last two
loads return 0x42 and 0x99, the bytes stored earlier, not immediate operands. The accumulator having
been cleared to zero in between is what makes the read-back unambiguous.

## Construction

- 6502_CPU_Core_6502 - the faithful CPU core, unchanged.
- PROGRAM - a ROM holding the code in its low 128 bytes.
- RAM ASYNC - the data memory; its combinational read matches the core's single-cycle bus.
- The address-bus low byte fans out to both memories; a small decode (address bit 7 and RW) gates the
  ROM read, the RAM read and the RAM write; and eight three-way tri-state buses form the one shared
  data bus, driven by the ROM, the RAM or the core depending on the cycle.

## Reference

6502 bus and memory interface: https://www.nesdev.org/wiki/CPU_memory_map
