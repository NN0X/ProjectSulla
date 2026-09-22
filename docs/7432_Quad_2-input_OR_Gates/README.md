# 7432 - Quad 2-input OR gate

Real gate-level implementation of the 7432 TTL logic IC: **4 independent OR gate(s)**,
each with 2 input(s). Logic per gate: `Y = A OR B`.

The layout (`layouts/7432_Quad_2-input_OR_Gates.json`) exposes every gate's inputs and output as pins
(8 inputs / 4 outputs), labelled A/B per gate and Y per output. It is
validated against this truth table in `make test` on both the interpreted and
native engines.

## Truth table (per gate)

| A | B | Y |
| --- | --- | --- |
| 0 | 0 | 0 |
| 0 | 1 | 1 |
| 1 | 0 | 1 |
| 1 | 1 | 1 |

## Reference

The function and pin behaviour summarised above are taken from the Texas Instruments
datasheet for the 7432 function. TI publishes it in the High-Speed CMOS family as
**SN74HC32**; the datasheet PDF is copyrighted by TI and is not redistributed here.
Official document: <https://www.ti.com/lit/ds/symlink/sn74hc32.pdf>.
