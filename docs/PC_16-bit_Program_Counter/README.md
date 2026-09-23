# 16-bit program counter

A 16-bit program counter built from four 74163 synchronous counters cascaded through
their ripple-carry chain (each stage's RCO enables the next stage's count input, so a
stage advances only when all lower stages are at 15). It is the address register that a
6502-class CPU steps through instruction memory: it increments on each fetch and loads a
new address on a jump or branch.

All actions happen on the rising CLK edge with the 74163 priority: synchronous clear
(nCLR=0 -> 0x0000), parallel load (nLOAD=0 -> Q=D), increment when CE is high, otherwise
hold. The four stages share CLK, nLOAD and nCLR; CE drives every stage's count-enable and
the carry ripples up the RCO chain so the full 16-bit value increments by one per clock.

**Interface:** 20 inputs (D0..D15, nLOAD, nCLR, CE, CLK) / 16 outputs (Q0..Q15).

## Reference

74163 (Texas Instruments SN74HC163): <https://www.ti.com/lit/ds/symlink/sn74hc163.pdf>
