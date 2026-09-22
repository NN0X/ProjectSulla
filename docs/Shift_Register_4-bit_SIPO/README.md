# Shift Register 4-bit SIPO - 4-bit shift register (serial-in, parallel-out)

Four positive-edge master-slave D flip-flops chained: on each clock edge the serial input Din shifts into Q0 and each stage passes its value to the next (Q0->Q1->Q2->Q3). Serial data in, parallel data out.

**Interface:** 2 inputs (Din, CLK) / 4 outputs (Q0..Q3).

**Validation:** checked by shifting a bit pattern through and checking the parallel outputs each clock against a golden model in `make test` on the interpreted
and both native engines. Compiled into `parts/` by `make fixtures`.

## Reference

Standard textbook digital-logic circuit (a real-world design, not a specific chip).
Background: <https://en.wikipedia.org/wiki/Shift_register>.
