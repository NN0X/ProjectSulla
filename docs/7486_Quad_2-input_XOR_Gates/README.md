# 7486 - Quad 2-input XOR gate

Real gate-level implementation of the 7486 TTL logic IC: **4 independent XOR gate(s)**,
each with 2 input(s). Logic per gate: `Y = A XOR B`.

The layout (`layouts/7486_Quad_2-input_XOR_Gates.json`) exposes every gate's inputs and output as pins
(8 inputs / 4 outputs), labelled A/B per gate and Y per output.

## Truth table (per gate)

| A | B | Y |
| --- | --- | --- |
| 0 | 0 | 0 |
| 0 | 1 | 1 |
| 1 | 0 | 1 |
| 1 | 1 | 0 |

## Reference

Texas Instruments SN74HC86 datasheet: <https://www.ti.com/lit/ds/symlink/sn74hc86.pdf>
