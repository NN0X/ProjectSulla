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
- Read port: four 74153 dual 4-to-1 multiplexers form an 8-bit 4-to-1 selector, choosing
  the addressed register's byte onto RD (each 74153 covers two bit positions).

The read port is a multiplexer that routes one register's outputs to RD; an alternative
realization gates each register onto a shared bus so the selected one drives it directly.

## Reference

MOS 6502 register set: https://en.wikipedia.org/wiki/MOS_Technology_6502
