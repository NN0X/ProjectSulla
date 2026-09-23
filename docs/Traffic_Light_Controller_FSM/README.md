# Traffic Light Controller FSM - Traffic-light controller (Moore FSM)

A finite-state machine driving a single traffic light through the cycle Green -> Yellow -> Red -> Green, one step per clock edge. A 2-bit state register (two edge flip-flops) holds the current state; next-state logic advances the cycle and Moore output logic lights exactly one lamp per state. An active-low reset (nRST) forces the Green state asynchronously.

**Interface:** 2 inputs (CLK, nRST) / 3 outputs (Green, Yellow, Red).

## Reference

Reference: <https://en.wikipedia.org/wiki/Finite-state_machine>
