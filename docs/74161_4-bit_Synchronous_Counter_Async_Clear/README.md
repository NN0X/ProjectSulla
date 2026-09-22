# 74161 - 4-bit synchronous binary counter (asynchronous clear)

Real gate-level 4-bit synchronous counter built from positive-edge master-slave D
flip-flops. It is identical to the 74163 except that its clear is **asynchronous**:
while `nCLR = 0` every Q is forced to 0 immediately, independent of the clock,
whereas the 74163 clears only on the rising edge. Async clear is implemented by
gating each flip-flop's master and slave outputs with nCLR (as in the 74273),
rather than gating the flip-flop's data input.

On the rising CLK edge (when not clearing): parallel load when `nLOAD = 0` (Q = D),
otherwise count up when both enables are high (ENP and ENT), otherwise hold. A
ripple-carry chain drives `RCO = ENT and (Q == 15)` for cascading.

**Interface:** 9 inputs (D0..D3, nLOAD, nCLR, ENP, ENT, CLK) / 5 outputs (Q0..Q3, RCO).

**Validation:** clocked through load, count, RCO, and specifically an asynchronous
clear asserted while the clock is HIGH with no rising edge (which a synchronous-clear
counter would ignore) - checked against a golden model on interpreted + both native
engines.

## Reference

The function and pin behaviour summarised above are taken from the Texas Instruments
datasheet for the 74161 function. TI publishes it in the High-Speed CMOS family as
**SN74HC161**; the datasheet PDF is copyrighted by TI and is not redistributed here.
Official document: <https://www.ti.com/lit/ds/symlink/sn74hc161.pdf>.
