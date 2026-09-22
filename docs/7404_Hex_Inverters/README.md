# 7404 - Hex inverter

Real gate-level implementation of the 7404 TTL logic IC: **6 independent NOT gate(s)**,
each with 1 input(s). Logic per gate: `Y = NOT A`.

The layout (`layouts/7404_Hex_Inverters.json`) exposes every gate's inputs and output as pins
(6 inputs / 6 outputs), labelled A/B per gate and Y per output. It is
validated against this truth table in `make test` on both the interpreted and
native engines.

## Truth table (per gate)

| A | Y |
| --- | --- |
| 0 | 1 |
| 1 | 0 |

## Reference

The function and pin behaviour summarised above are taken from the Texas Instruments
datasheet for the 7404 function. TI publishes it in the High-Speed CMOS family as
**SN74HC04**; the datasheet PDF is copyrighted by TI and is not redistributed here.
Official document: <https://www.ti.com/lit/ds/symlink/sn74hc04.pdf>.
