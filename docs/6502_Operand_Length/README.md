# 6502 Operand Length (cc = 01 group)

Decodes how many operand bytes follow the opcode for the cc = 01 instruction group - the
accumulator ALU instructions ORA, AND, EOR, ADC, STA, LDA, CMP and SBC. The control unit uses
this to know how many more bytes to fetch after the opcode, and so how many cycles the
instruction runs.

For this group the count depends only on the addressing-mode field, so the decode is regular:
the immediate, zero-page and indexed-zero-page and indirect modes take one operand byte, and the
three absolute modes take two.

## Interface

Inputs (8):
- O0..O7 - the opcode

Outputs (3):
- OB0, OB1 - the operand-byte count in binary (OB1 OB0 = 0, 1 or 2)
- CC01     - high when the opcode is in the cc = 01 group (the count is meaningful)

## Behaviour

    CC01 = 1 when the opcode's low two bits are 01

For a cc = 01 opcode the addressing-mode field is opcode bits 4, 3, 2. The absolute modes -
absolute, absolute,X and absolute,Y - carry a two-byte address, everything else a single byte:

    two operand bytes when bit 3 is set and either bit 2 or bit 4 is set
    one operand byte  otherwise

For any opcode outside the group the count is zero and CC01 is low.

## Construction

A cc = 01 detect ANDs opcode bit 0 with the inverse of bit 1. The two-byte-mode term is bit 3
ANDed with the OR of bits 2 and 4. The count-of-two output is the group detect ANDed with that
term; the count-of-one output is the group detect ANDed with its inverse.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
