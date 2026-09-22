# ROM - read-only memory primitive (16 x 8 lookup-table fixture)

ROM is a **new first-class primitive** (`PART_TYPE_ROM`), not a circuit built from
gates. It is a combinational lookup table: the address presented on its inputs
selects one stored word, which appears on its data outputs. With A address inputs
and W data outputs it holds 2^A words of W bits.

Its contents live with the part (the `romData` field in the layout JSON), so a ROM is
self-contained and serialises with the circuit. Contents are meant to be populated
from a **hex dump**: `parseHexDump` turns whitespace/newline-separated hex tokens into
words (masked to W bits), which the GUI wires to the native file picker (1.12) so a
program or table can be loaded from disk.

This fixture is a 16 x 8 ROM (4 address inputs, 8 data outputs) preloaded so that
`word[i] = 0xA0 + i`, giving an easy-to-verify table.

**Interface:** A address inputs / W data outputs (here 4 in / 8 out); purely
combinational (no clock).

**Validation:** every address is read back and checked against its stored word on the
interpreted engine, and `parseHexDump` is unit-tested, in `make test`.

## Status / remaining work

- DONE: the primitive itself (type, `makeRomPart`), JSON serialisation of `romData`,
  loading into the interpreted engine, and hex parsing - all validated.
- TODO (mapped in the plan): native-engine transpilation (emit the contents as a
  static table + address lookup in the compiled path - needs `romData` threaded
  through FlatCircuit and the two emit sites in compiler/core.cpp) and the GUI
  surface (sidebar entry, draw, getPartSize, place, and the "load hex" action wired to
  openNativeFileDialog -> parseHexDump). Until native transpile lands, ROM fixtures are
  kept in tests/layouts/ (interpreted-tested) rather than layouts/.

## Reference

A ROM/lookup-table is a standard digital building block; background:
<https://en.wikipedia.org/wiki/Read-only_memory>. This is a generic educational ROM
primitive rather than a replica of a specific chip.
