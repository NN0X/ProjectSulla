# 6502 Flag Operation Decoder

Decodes the 6502 single-byte flag set/clear instructions into the control the P status
register needs: a per-bit load enable (L) and the value to load (F). These instructions
live in the cc = 00, bbb = 110 column (opcodes ending in $8), selected by the aaa field:

    opcode  aaa  instruction  effect
    $18     000  CLC          C := 0
    $38     001  SEC          C := 1
    $58     010  CLI          I := 0
    $78     011  SEI          I := 1
    $98     100  (TYA)        not a flag op - no output
    $B8     101  CLV          V := 0
    $D8     110  CLD          D := 0
    $F8     111  SED          D := 1

## Interface

Inputs (8):
- O0..O7 - the opcode

Outputs (16):
- F0..F7 - flag values to present to the P register
- L0..L7 - per-bit load enables for the P register

Only the bits these instructions can touch are ever driven: C (bit 0), I (bit 2),
D (bit 3) and V (bit 6). Every other F and L output is held low, so this decoder can be
combined with the other flag sources (the ALU result path sets N, Z, C, V) by OR-ing the
load enables.

## Behaviour

For a matching instruction, the target flag's L bit is raised and its F bit carries the
new value (1 for SEC/SEI/SED, 0 for CLC/CLI/CLD/CLV); wired to the P register, one clock
then updates exactly that flag and leaves the others unchanged. For any other opcode all
outputs are low, so the P register holds.

## Construction

A two-level gate PLA. The AND plane forms one product term per instruction: the column
detector (cc = 00 and bbb = 110) AND-ed with the three aaa literals. The OR plane routes
those terms to the F and L outputs of the flags they affect; untouched outputs are tied
to a constant low.

## Reference

6502 opcode matrix: https://www.masswerk.at/6502/6502_instruction_set.html
