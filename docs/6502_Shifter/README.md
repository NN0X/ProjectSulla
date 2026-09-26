# 6502 Shifter

A single-place shifter for the accumulator shift and rotate instructions. It moves the input
byte one position left or right, brings in either a zero or the carry flag at the vacated end,
and reports the bit that falls off the other end as the carry out. The four cc = 10 accumulator
operations are the four combinations of direction and fill:

    DIR  ROT   operation
    0    0     ASL   shift left,  fill 0
    0    1     ROL   rotate left  through carry
    1    0     LSR   shift right, fill 0
    1    1     ROR   rotate right through carry

## Interface

Inputs (11):
- D0..D7 - the byte to shift
- Cin    - the carry flag (brought in on a rotate)
- DIR    - direction: 0 = left, 1 = right
- ROT    - fill: 0 = shift in 0, 1 = rotate in Cin

Outputs (9):
- R0..R7 - the shifted byte
- Cout   - the bit shifted out (bit 7 on a left shift, bit 0 on a right shift)

## Behaviour

    fill = ROT AND Cin                 (Cin on a rotate, 0 on a shift)

    left  (DIR = 0): R = (D << 1) with bit 0 = fill,  Cout = D7
    right (DIR = 1): R = (D >> 1) with bit 7 = fill,  Cout = D0

## Construction

Gate level. Each result bit is a two-way multiplexer selected by DIR between its left neighbour
(the bit below, or the fill at bit 0) and its right neighbour (the bit above, or the fill at
bit 7). Cout is the same multiplexer selecting D7 for a left shift and D0 for a right shift. The
fill is one AND of ROT and Cin.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
