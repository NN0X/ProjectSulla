# 6502 Transfer Decoder

Decodes the 6502 register-to-register transfer instructions into the control the register
file needs: which register to read, which to write, and the write enable. In the register
file the index order is 0 = A, 1 = X, 2 = Y, 3 = SP.

    opcode  instruction  move
    $AA     TAX          A  -> X
    $8A     TXA          X  -> A
    $A8     TAY          A  -> Y
    $98     TYA          Y  -> A
    $BA     TSX          SP -> X
    $9A     TXS          X  -> SP

## Interface

Inputs (8):
- O0..O7 - the opcode

Outputs (5):
- RS0, RS1 - read select (the source register)
- WS0, WS1 - write select (the destination register)
- WE       - write enable (high for a recognized transfer, low otherwise)

## Behaviour

For a recognized transfer the read select addresses the source register and the write
select the destination, with WE high; wired to the register file, the source register's
value is read out and latched into the destination on the next clock. For any other
opcode WE is low, so the register file holds.

## Construction

A two-level gate PLA. Because these opcodes are irregular (they sit in the implied-mode
columns and do not share a single field pattern), the AND plane forms one product term per
instruction from a full eight-bit opcode match. The OR plane routes those terms to the
read-select, write-select and write-enable lines according to each transfer's source and
destination.

## Reference

6502 opcode matrix: https://www.masswerk.at/6502/6502_instruction_set.html
