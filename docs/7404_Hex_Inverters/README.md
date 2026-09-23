# 7404 - Hex inverter

Real gate-level implementation of the 7404 TTL logic IC: **6 independent NOT gate(s)**,
each with 1 input(s). Logic per gate: `Y = NOT A`.

The layout (`layouts/7404_Hex_Inverters.json`) exposes every gate's inputs and output as pins
(6 inputs / 6 outputs), labelled A/B per gate and Y per output.

## Truth table (per gate)

| A | Y |
| --- | --- |
| 0 | 1 |
| 1 | 0 |

## Reference

Texas Instruments SN74HC04 datasheet: <https://www.ti.com/lit/ds/symlink/sn74hc04.pdf>
