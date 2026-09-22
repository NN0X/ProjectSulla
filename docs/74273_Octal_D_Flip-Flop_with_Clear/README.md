# 74273 - octal D-type flip-flop with clear

Real gate-level implementation of the 74273 8-bit register, built entirely from
2-input primitive gates. Each of the 8 bits is a **positive-edge-triggered
master-slave D flip-flop**: a master latch (transparent while CLK is LOW) feeding
a slave latch (transparent while CLK is HIGH), so Q captures D on the rising edge
of CLK and holds it otherwise. A common active-low clear (nCLR) asynchronously
forces every Q to 0 regardless of the clock.

**Interface:** 10 inputs (D0..D7, CLK, nCLR) / 8 outputs (Q0..Q7).

**Behaviour:** on each CLK rising edge, `Q = D`; while CLK is steady (high or low),
Q holds; while `nCLR = 0`, `Q = 0` (asynchronous). Changing D while CLK is HIGH
does not disturb Q - it is edge-triggered, not level-sensitive.

**Validation:** clocked through a mixed sequence of loads, holds and an async clear
and checked against a golden edge-triggered model in `make test` on all three
engines (interpreted, native inline, native link). It is compiled into `parts/`
by `make fixtures`.

## Reference

The function and pin behaviour summarised above are taken from the Texas Instruments
datasheet for the 74273 function. TI publishes it in the High-Speed CMOS family as
**SN74HC273**; the datasheet PDF is copyrighted by TI and is not redistributed here.
Official document: <https://www.ti.com/lit/ds/symlink/sn74hc273.pdf>.
