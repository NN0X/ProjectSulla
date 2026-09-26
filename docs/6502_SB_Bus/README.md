# 6502 Internal SB Bus

The datapath's shared internal bus. Several sources - a register read out of the register file,
data arriving from memory, and a result from the ALU - each gate onto one shared 8-bit bus
through tri-state buffers, and a source select enables exactly one of them, so the bus carries
whichever source the control unit chooses. This is the spine the datapath units drive and read
instead of point-to-point wiring.

## Interface

Inputs (32):
- WD0..WD7 - register write data
- WS0, WS1 - register write select
- WE       - register write enable
- RS0, RS1 - register read select (which register drives the bus in register mode)
- DIN0..DIN7 - data from memory
- AIN0..AIN7 - result from the ALU
- SRC0, SRC1 - the bus source select
- CLK      - the clock

Outputs (8):
- SB0..SB7 - the shared bus

## Behaviour

The register file holds A, X, Y and SP and is written through WD/WS/WE as usual. The source
select routes one source onto the bus:

    SRC = 00  register file read-out (the register RS addresses)
    SRC = 01  memory data (DIN)
    SRC = 10  ALU result (AIN)
    SRC = 11  no source enabled - the bus floats and reads as 0

Exactly one source's buffers are enabled at a time; the rest are high-impedance, so the bus
holds the selected source's byte. With no source selected the bus floats, which resolves to 0.

## Construction

- Register file: A/X/Y/SP with its tri-state read port, one of the bus sources.
- Source decode: the two select bits become three one-hot enables, one per source.
- Bus: for each of the eight bit positions, the register read-out bit, the memory bit and the ALU
  bit pass through tri-state buffers - each enabled by its source's decode - onto a shared bus,
  and the eight buses are SB. Since at most one enable is high, at most one buffer drives each bit.

## Reference

MOS 6502 internal architecture: https://www.nesdev.org/wiki/Visual6502wiki/6502_datapath
