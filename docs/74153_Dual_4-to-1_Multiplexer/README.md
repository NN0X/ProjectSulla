# 74153 - dual 4-to-1 line multiplexer

Real gate-level implementation of the 74153 MSI TTL part, built entirely from
2-input primitive gates.

Two 4-to-1 multiplexers sharing two select lines A,B. Each mux d selects one of its four data inputs `dC0..dC3` by the binary value `A + 2*B`, gated by its own active-low enable dG (output LOW when disabled).

**Interface:** 12 inputs (1C0..1C3, 2C0..2C3, A, B, 1G, 2G) / 2 outputs (1Y, 2Y).

## Reference

Texas Instruments SN74HC153 datasheet: <https://www.ti.com/lit/ds/symlink/sn74hc153.pdf>
