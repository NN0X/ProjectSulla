# 74163 - 4-bit synchronous binary counter

Real gate-level 4-bit synchronous counter built from positive-edge master-slave D
flip-flops. All actions happen on the rising CLK edge with this priority:
synchronous clear (nCLR=0 -> 0), parallel load (nLOAD=0 -> Q=D), count up when both
enables are high (ENP and ENT), otherwise hold. A ripple-carry chain drives
`RCO = ENT and (Q == 15)` for cascading. This is the synchronous-clear variant
(the 74161 differs only in having an asynchronous clear).

**Interface:** 9 inputs (D0..D3, nLOAD, nCLR, ENP, ENT, CLK) / 5 outputs (Q0..Q3, RCO).

**Validation:** clocked through clear, load, a full count with 15->0 rollover, RCO assertion and a disabled-hold, checked on interpreted + both native engines. Compiled into `parts/` by `make fixtures`.

## Reference

The function and pin behaviour summarised above are taken from the Texas Instruments
datasheet for the 74163 function. TI publishes it in the High-Speed CMOS family as
**SN74HC163**; the datasheet PDF is copyrighted by TI and is not redistributed here.
Official document: <https://www.ti.com/lit/ds/symlink/sn74hc163.pdf>.
