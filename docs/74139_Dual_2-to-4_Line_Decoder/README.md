# 74139 - dual 2-to-4 line decoder / demultiplexer

Real gate-level implementation of the 74139 MSI TTL part, built entirely from
2-input primitive gates.

Two independent decoders. Each takes two select lines (A,B) and an active-low enable (G) and drives four active-low outputs Y0..Y3: when enabled, exactly the output whose index equals the binary value `A + 2*B` is driven LOW and the rest HIGH; when disabled (G high) all outputs are HIGH.

**Interface:** 6 inputs (1A,1B,1G, 2A,2B,2G) / 8 outputs (1Y0..1Y3, 2Y0..2Y3).

## Reference

Texas Instruments SN74HC139 datasheet: <https://www.ti.com/lit/ds/symlink/sn74hc139.pdf>
