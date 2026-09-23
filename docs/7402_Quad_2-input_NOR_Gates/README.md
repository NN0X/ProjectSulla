# 7402 - Quad 2-input NOR gate

Real gate-level implementation of the 7402 TTL logic IC: **4 independent NOR gate(s)**,
each with 2 input(s). Logic per gate: `Y = NOT (A OR B)`.

The layout (`layouts/7402_Quad_2-input_NOR_Gates.json`) exposes every gate's inputs and output as pins
(8 inputs / 4 outputs), labelled A/B per gate and Y per output.

## Truth table (per gate)

| A | B | Y |
| --- | --- | --- |
| 0 | 0 | 1 |
| 0 | 1 | 0 |
| 1 | 0 | 0 |
| 1 | 1 | 0 |

## Reference

Texas Instruments SN74HC02 datasheet: <https://www.ti.com/lit/ds/symlink/sn74hc02.pdf>
