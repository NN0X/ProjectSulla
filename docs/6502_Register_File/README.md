# 6502 Register File

The four 8-bit general registers of the MOS 6502 - A (accumulator), X and Y (index
registers) and SP (stack pointer) - gathered as a register file with one write port
and one read port. Register index 0 = A, 1 = X, 2 = Y, 3 = SP.

## Interface

Inputs (14):
- WD0..WD7 - write data
- WS0, WS1 - write select (which register the write targets)
- WE       - write enable
- RS0, RS1 - read select (which register appears on the read port)
- CLK      - clock; writes take effect on the rising edge

Outputs (8):
- RD0..RD7 - the selected register's contents (combinational read)

## Behaviour

On each rising clock edge, if WE is high the register selected by WS loads WD; every
other register holds. The register selected by RS is always presented on RD, so a read
is combinational and independent of the clock:

    if WE:  reg[WS] <- WD        (on rising CLK)
    RD      =  reg[RS]           (any time)

## Construction

- Storage: four 74377 octal D registers, one per register, all fed the common write bus WD.
- Write select: one half of a 74139 2-to-4 decoder turns WS into four active-low
  selects; each is OR-ed with the inverted write enable to drive that register's
  active-low load enable, so a register loads only when it is selected and WE is high.
- Read port: a small decoder turns RS into four one-hot read enables, one per register. Each
  register's eight output bits pass through tri-state buffers - enabled by that register's read
  enable - onto eight shared buses, one per bit position, which form RD. With exactly one read
  enable high, only that register drives the buses; the others are high-impedance, so RD carries
  the addressed register's byte.

This is the faithful shared-bus read port: each register gates its outputs onto the read bus and
the selected one drives it, rather than a multiplexer routing one register's outputs across.

## Reference

MOS 6502 register set: https://en.wikipedia.org/wiki/MOS_Technology_6502
