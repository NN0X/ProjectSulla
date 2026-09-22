# 7408 - Quad 2-input AND gate

Real gate-level implementation of the 7408 TTL logic IC: **4 independent AND gate(s)**,
each with 2 input(s). Logic per gate: `Y = A AND B`.

The layout (`layouts/7408_Quad_2-input_AND_Gates.json`) exposes every gate's inputs and output as pins
(8 inputs / 4 outputs), labelled A/B per gate and Y per output. It is
validated against this truth table in `make test` on both the interpreted and
native engines.

## Truth table (per gate)

| A | B | Y |
| --- | --- | --- |
| 0 | 0 | 0 |
| 0 | 1 | 0 |
| 1 | 0 | 0 |
| 1 | 1 | 1 |

## Reference

The function and pin behaviour summarised above are taken from the Texas Instruments
datasheet for the 7408 function. TI publishes it in the High-Speed CMOS family as
**SN74HC08**; the datasheet PDF is copyrighted by TI and is not redistributed here.
Official document: <https://www.ti.com/lit/ds/symlink/sn74hc08.pdf>.
