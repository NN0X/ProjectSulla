# 74377 - octal D-type register with clock enable

Real gate-level 8-bit register built from positive-edge-triggered master-slave D
flip-flops. A common active-low enable (nE) gates loading: while `nE = 0` the
register loads D on each rising CLK edge; while `nE = 1` it holds, ignoring clock
edges. Implemented by muxing each flip-flop's data input between D (when enabled)
and its own Q (when disabled).

**Interface:** 10 inputs (D0..D7, CLK, nE) / 8 outputs (Q0..Q7).

## Reference

Texas Instruments SN74HC377 datasheet: <https://www.ti.com/lit/ds/symlink/sn74hc377.pdf>
