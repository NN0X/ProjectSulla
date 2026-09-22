# Traffic Light Controller FSM - Traffic-light controller (Moore FSM)

A finite-state machine driving a single traffic light through the cycle Green -> Yellow -> Red -> Green, one step per clock edge. A 2-bit state register (two edge flip-flops) holds the current state; next-state logic advances the cycle and Moore output logic lights exactly one lamp per state. An active-low reset (nRST) forces the Green state asynchronously.

**Interface:** 2 inputs (CLK, nRST) / 3 outputs (Green, Yellow, Red).

**Validation:** checked by clocking through the full cycle with a reset and checking exactly one lamp is lit per state against a golden model in `make test` on the interpreted
and both native engines. Compiled into `parts/` by `make fixtures`.

## Reference

Standard textbook digital-logic circuit (a real-world design, not a specific chip).
Background: <https://en.wikipedia.org/wiki/Finite-state_machine>.
