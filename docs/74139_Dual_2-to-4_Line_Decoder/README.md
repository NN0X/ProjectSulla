# 74139 - dual 2-to-4 line decoder / demultiplexer

Real gate-level implementation of the 74139 MSI TTL part, built entirely from
2-input primitive gates.

Two independent decoders. Each takes two select lines (A,B) and an active-low enable (G) and drives four active-low outputs Y0..Y3: when enabled, exactly the output whose index equals the binary value `A + 2*B` is driven LOW and the rest HIGH; when disabled (G high) all outputs are HIGH.

**Interface:** 6 inputs (1A,1B,1G, 2A,2B,2G) / 8 outputs (1Y0..1Y3, 2Y0..2Y3).

**Validation:** Exhaustively validated over all 64 input combinations against the active-low one-hot decode with enable. It is compiled into `parts/` by `make fixtures` and checked
in `make test` on both the interpreted and native engines.

## Reference

Texas Instruments 74139 datasheet (SN7474139). The datasheet PDF is copyrighted by
Texas Instruments and is **not redistributed here**; see the official document at
<https://www.ti.com/lit/gpn/sn7474139>. The function and pin behaviour summarised
above are factual and taken from that specification.
