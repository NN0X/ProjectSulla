# 74377 - octal D-type register with clock enable

Real gate-level 8-bit register built from positive-edge-triggered master-slave D
flip-flops. A common active-low enable (nE) gates loading: while `nE = 0` the
register loads D on each rising CLK edge; while `nE = 1` it holds, ignoring clock
edges. Implemented by muxing each flip-flop's data input between D (when enabled)
and its own Q (when disabled).

**Interface:** 10 inputs (D0..D7, CLK, nE) / 8 outputs (Q0..Q7).

**Validation:** clocked through a load/hold/enable sequence and checked against a golden model on interpreted + both native engines. Compiled into `parts/` by `make fixtures`.

## Reference

Texas Instruments 74377 datasheet (SN7474377). The datasheet PDF is copyrighted by
Texas Instruments and is **not redistributed here**; see the official document at
<https://www.ti.com/lit/gpn/sn7474377>. The function and pin behaviour summarised
above are factual and taken from that specification.
