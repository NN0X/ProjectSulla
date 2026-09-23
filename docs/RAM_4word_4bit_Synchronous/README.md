# RAM - 4-word x 4-bit synchronous read/write memory

A gate-level static RAM: 4 addressable words of 4 bits each, built entirely from
2-input primitive gates. It demonstrates the three classic building blocks of a
memory working together:

- **Address decode** - a 2-to-4 one-hot decoder turns the address (A0, A1) into a
  per-word select line (`sel = A0 + 2*A1`).
- **Storage** - each of the 16 cells is a positive-edge master-slave D flip-flop with
  a per-word write-enable. On the rising CLK edge a cell captures Din only when
  `WE = 1` and its word is selected; otherwise it holds.
- **Read mux** - the data outputs are an asynchronous (combinational) mux: for each
  bit, `Dout = OR over words of (cell AND sel)`, which selects the addressed word's
  bits directly (no clock needed to read).

So writes are synchronous (edge-triggered, gated by WE and address) and reads are
asynchronous - the same behaviour as a small classic static RAM such as the 7489
(16x4) or 2114 (1Kx4), scaled down to keep the gate count viewable.

**Interface:** 8 inputs (A0, A1, Din0..Din3, WE, CLK) / 4 outputs (Dout0..Dout3).

## Reference

Reference: <https://en.wikipedia.org/wiki/Static_random-access_memory>
