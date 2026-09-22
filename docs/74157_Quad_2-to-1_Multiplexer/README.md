# 74157 - quad 2-to-1 line multiplexer

Real gate-level implementation of the 74157 MSI TTL part, built entirely from
2-input primitive gates.

Four 2-to-1 multiplexers sharing one select line S and one active-low enable G. For each bit i, `Yi = Ai` when S is LOW and `Yi = Bi` when S is HIGH; when G is HIGH all outputs are forced LOW.

**Interface:** 10 inputs (A1..A4, B1..B4, S, G) / 4 outputs (Y1..Y4).

**Validation:** Exhaustively validated over all 1024 input combinations. It is compiled into `parts/` by `make fixtures` and checked
in `make test` on both the interpreted and native engines.

## Reference

The function and pin behaviour summarised above are taken from the Texas Instruments
datasheet for the 74157 function. TI publishes it in the High-Speed CMOS family as
**SN74HC157**; the datasheet PDF is copyrighted by TI and is not redistributed here.
Official document: <https://www.ti.com/lit/ds/symlink/sn74hc157.pdf>.
