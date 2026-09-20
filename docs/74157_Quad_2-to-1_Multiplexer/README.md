# 74157 - quad 2-to-1 line multiplexer

Real gate-level implementation of the 74157 MSI TTL part, built entirely from
2-input primitive gates.

Four 2-to-1 multiplexers sharing one select line S and one active-low enable G. For each bit i, `Yi = Ai` when S is LOW and `Yi = Bi` when S is HIGH; when G is HIGH all outputs are forced LOW.

**Interface:** 10 inputs (A1..A4, B1..B4, S, G) / 4 outputs (Y1..Y4).

**Validation:** Exhaustively validated over all 1024 input combinations. It is compiled into `parts/` by `make fixtures` and checked
in `make test` on both the interpreted and native engines.

## Reference

Texas Instruments 74157 datasheet (SN7474157). The datasheet PDF is copyrighted by
Texas Instruments and is **not redistributed here**; see the official document at
<https://www.ti.com/lit/gpn/sn7474157>. The function and pin behaviour summarised
above are factual and taken from that specification.
