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

Texas Instruments 7404 datasheet (SN747404). The datasheet PDF is copyrighted by
Texas Instruments and is therefore **not redistributed here**; see the official
document at <https://www.ti.com/lit/gpn/sn747404>. The truth table and function
above are factual and taken from that specification.
