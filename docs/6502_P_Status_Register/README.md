# 6502 P Status Register

The processor status register of the MOS 6502: an 8-bit register holding the CPU
condition flags. Its defining feature is that each bit can be updated independently -
different operations affect different flags - so it is an 8-bit register with a
per-bit load enable rather than a plain load-all register.

Bit assignment (bit 7 down to bit 0): N V - B D I Z C
(Negative, Overflow, unused, Break, Decimal, Interrupt-disable, Zero, Carry).

## Interface

Inputs (17):
- F0..F7 - new flag values to load into each bit
- L0..L7 - per-bit load enable (bit i loads F i when L i is high, else holds)
- CLK    - clock; updates happen on the rising edge

Outputs (8):
- Q0..Q7 - the stored flag bits

## Behaviour

On each rising clock edge, for every bit i independently:

    Q[i] <- F[i]        if L[i] = 1
    Q[i] <- Q[i]        if L[i] = 0   (hold)

This lets one operation set the carry while another updates negative and zero, each
without disturbing the others.

## Construction

Storage is a 74377 octal D register with clock enable, wired to load on every clock
(its active-low enable is held low). Per-bit independence is added with a hold/load
multiplexer on each data input:

    D[i] = (L[i] AND F[i]) OR (NOT L[i] AND Q[i])

so a bit whose load enable is low is refreshed with its own current value and a bit
whose load enable is high takes the incoming flag value.

## Reference

74377 octal D flip-flop with enable: https://www.ti.com/lit/ds/symlink/sn74hc377.pdf
