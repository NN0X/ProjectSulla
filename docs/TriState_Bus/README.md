# Tri-State Bus

Exercises the tri-state buffer and bus primitives: three tri-state buffers driving one shared
bus. Each buffer passes its data onto the bus when enabled and releases the bus (high impedance)
when disabled; the bus resolves whatever its drivers present into a single value.

## Interface

Inputs (6):
- D0, D1, D2 - the three buffers' data
- E0, E1, E2 - the three buffers' enables

Outputs (1):
- Q - the resolved bus value

## Behaviour

A buffer drives its data bit onto the bus while its enable is high, and floats (drives nothing)
while its enable is low. The bus resolves its drivers:

    no driver enabled          -> 0   (floating reads as 0)
    exactly one driver         -> that driver's value
    several drivers, agreeing  -> that value
    several drivers, differing -> 0   (contention)

Equivalently, the bus is high when some driver drives high and none drives low:

    Q = (any enabled buffer drives 1) AND NOT (any enabled buffer drives 0)

## Resolution across the engines

The three engines agree on every case, contention included. The interpreter reads the buffers'
three-state outputs (high, low or high-impedance) and resolves them. Both native paths - the
scalar per-signal path and the bitsliced SIMD path - reach through each buffer to its data and
enable and compute the same resolution with plain bit operations, so a signal stays one bit wide
and the SIMD path keeps its full width. A bus with a single active driver - the only kind a
correct design produces - always reads back that driver's value.

## Reference

Tri-state logic and buses: https://en.wikipedia.org/wiki/Three-state_logic
