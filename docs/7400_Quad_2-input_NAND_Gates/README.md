# 7400 - Quad 2-input NAND gate

Real gate-level implementation of the 7400 TTL logic IC: **4 independent NAND gate(s)**,
each with 2 input(s). Logic per gate: `Y = NOT (A AND B)`.

The layout (`layouts/7400_Quad_2-input_NAND_Gates.json`) exposes every gate's inputs and output as pins
(8 inputs / 4 outputs), labelled A/B per gate and Y per output. It is
validated against this truth table in `make test` on both the interpreted and
native engines.

## Truth table (per gate)

| A | B | Y |
| --- | --- | --- |
| 0 | 0 | 1 |
| 0 | 1 | 1 |
| 1 | 0 | 1 |
| 1 | 1 | 0 |

## Reference

The function and pin behaviour summarised above are taken from the Texas Instruments
datasheet for the 7400 function. TI publishes it in the High-Speed CMOS family as
**SN74HC00**; the datasheet PDF is copyrighted by TI and is not redistributed here.
Official document: <https://www.ti.com/lit/ds/symlink/sn74hc00.pdf>.
