# D Latch Gated - Gated D latch

A level-sensitive data latch: while the enable EN is high the output Q follows D (transparent); while EN is low Q holds its value. Built as `Q = (D AND EN) OR (Q AND NOT EN)`. The building block of registers and the transparent half of a flip-flop.

**Interface:** 2 inputs (D, EN) / 1 output (Q).

**Validation:** checked over a load/hold/load sequence against a golden model in `make test` on the interpreted
and both native engines. Compiled into `parts/` by `make fixtures`.

## Reference

Standard textbook digital-logic circuit (a real-world design, not a specific chip).
Background: <https://en.wikipedia.org/wiki/Flip-flop_(electronics)#Gated_D_latch>.
