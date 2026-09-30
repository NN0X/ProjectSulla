# RAM primitives (async and sync)

Two first-class writable-memory primitives, `RAM ASYNC` and `RAM SYNC`, alongside the read-only ROM
primitive. Each is a host-backed memory: an address selects a word, a write-enable stores the data
bus into that word, and the word is read back out. They replace the earlier label-encoded RAM (a
`RAM_SYNC_<addr>_<data>` convention on a CUSTOM part), which was not a real part type, had no sidebar
entry, and no glyph.

## Interface

For an A-bit address and a W-bit word the part has 1 + A + W inputs and W outputs:

- pin 0        - WE, the write-enable: when high, the data-in word is stored into the addressed word
- pins 1..A    - the address
- pins 1+A..   - the data-in word (W bits)
- outputs 0..W-1 - the data-out word

The address and data widths are taken from the pin counts (W = number of outputs, A = inputs - 1 - W),
the same way ROM derives its widths, so no extra metadata is stored. Dropped from the parts library a
RAM defaults to a 4-bit address and an 8-bit word (16 words). Memory starts cleared to zero.

## Behaviour

The write is the same for both: when WE is high the addressed word takes the data-in bus. The two
primitives differ only in the read:

- `RAM ASYNC` - combinational read: the data-out bus is the addressed word in the same tick, exactly
  like the ROM read. This is the read that matches a 6502-style bus, where the address settles and the
  data appears in the same cycle.
- `RAM SYNC`  - registered read: the data-out bus is the word that was addressed on the previous tick,
  and each tick latches the currently addressed word for the next one. This models a clocked memory
  with a one-tick read latency.

A read and a write in the same tick return the pre-write word (the read is taken before the write
lands), matching a real SRAM's read-before-write on a shared cycle.

## Glyphs

Each memory primitive is visually distinct: ROM is a gold checkerboard grid, `RAM ASYNC` a
teal grid of hollow cells, and `RAM SYNC` a violet grid of filled cells with a clock-edge triangle
below it (marking the registered, clocked read).

## Engines

Both primitives run on all three engines - the interpreter, the native inline transpiler and the
native link path - with the memory held as a host buffer (a `std::vector` in the interpreter, a static
array in the generated native code), so a large memory costs a buffer rather than the millions of
gates a gate-level SRAM of the same size would need. Validated by `testRamPrimitives`: a write/read
sequence over an 8-bit address and 8-bit word, checked against a per-tick model of each read style and
agreeing across all three engines.
