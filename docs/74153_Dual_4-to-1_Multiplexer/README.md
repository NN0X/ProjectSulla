# 74153 - dual 4-to-1 line multiplexer

Real gate-level implementation of the 74153 MSI TTL part, built entirely from
2-input primitive gates.

Two 4-to-1 multiplexers sharing two select lines A,B. Each mux d selects one of its four data inputs `dC0..dC3` by the binary value `A + 2*B`, gated by its own active-low enable dG (output LOW when disabled).

**Interface:** 12 inputs (1C0..1C3, 2C0..2C3, A, B, 1G, 2G) / 2 outputs (1Y, 2Y).

**Validation:** Exhaustively validated over all 4096 input combinations. It is compiled into `parts/` by `make fixtures` and checked
in `make test` on both the interpreted and native engines.

## Reference

Texas Instruments 74153 datasheet (SN7474153). The datasheet PDF is copyrighted by
Texas Instruments and is **not redistributed here**; see the official document at
<https://www.ti.com/lit/gpn/sn7474153>. The function and pin behaviour summarised
above are factual and taken from that specification.
