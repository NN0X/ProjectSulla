# D Latch Gated - Gated D latch

A level-sensitive data latch: while the enable EN is high the output Q follows D (transparent); while EN is low Q holds its value. Built as `Q = (D AND EN) OR (Q AND NOT EN)`. The building block of registers and the transparent half of a flip-flop.

**Interface:** 2 inputs (D, EN) / 1 output (Q).

## Reference

Reference: <https://en.wikipedia.org/wiki/Flip-flop_(electronics)#Gated_D_latch>
