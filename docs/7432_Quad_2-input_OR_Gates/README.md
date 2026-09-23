# 7432 - Quad 2-input OR gate

Real gate-level implementation of the 7432 TTL logic IC: **4 independent OR gate(s)**,
each with 2 input(s). Logic per gate: `Y = A OR B`.

The layout (`layouts/7432_Quad_2-input_OR_Gates.json`) exposes every gate's inputs and output as pins
(8 inputs / 4 outputs), labelled A/B per gate and Y per output.

## Truth table (per gate)

| A | B | Y |
| --- | --- | --- |
| 0 | 0 | 0 |
| 0 | 1 | 1 |
| 1 | 0 | 1 |
| 1 | 1 | 1 |

## Reference

Texas Instruments SN74HC32 datasheet: <https://www.ti.com/lit/ds/symlink/sn74hc32.pdf>
