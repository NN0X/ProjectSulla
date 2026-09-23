# 7408 - Quad 2-input AND gate

Real gate-level implementation of the 7408 TTL logic IC: **4 independent AND gate(s)**,
each with 2 input(s). Logic per gate: `Y = A AND B`.

The layout (`layouts/7408_Quad_2-input_AND_Gates.json`) exposes every gate's inputs and output as pins
(8 inputs / 4 outputs), labelled A/B per gate and Y per output.

## Truth table (per gate)

| A | B | Y |
| --- | --- | --- |
| 0 | 0 | 0 |
| 0 | 1 | 0 |
| 1 | 0 | 0 |
| 1 | 1 | 1 |

## Reference

Texas Instruments SN74HC08 datasheet: <https://www.ti.com/lit/ds/symlink/sn74hc08.pdf>
