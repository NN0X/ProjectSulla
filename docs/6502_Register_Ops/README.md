# 6502 Register Ops

Loads and register-to-register transfers over the register file, so A, X and Y are equal
citizens on the register bus. An immediate load writes the operand into one of the registers; a
transfer reads one register and writes it into another. The write data is selected on a shared
tri-state bus - the immediate for a load, the read-out register for a transfer.

## Interface

Inputs (19):
- O0..O7   - the opcode
- M0..M7   - the immediate operand (for loads)
- RSx0, RSx1 - the register to read out (for observation, when the opcode is neither a load
             nor a transfer)
- CLK      - the clock

Outputs (8):
- RD0..RD7 - the register file read port

## Behaviour

    LDA #  -> A = operand        LDX #  -> X = operand        LDY #  -> Y = operand
    TAX -> X = A    TXA -> A = X    TAY -> Y = A    TYA -> A = Y    TSX -> X = SP    TXS -> SP = X

A load's destination comes from the opcode group (cc = 01 selects A, cc = 10 selects X, cc = 00
selects Y). A transfer's source and destination come from the transfer decoder. The write enable
is high for a load or a transfer and low otherwise, so any other opcode leaves the registers
unchanged; the read select then addresses RSx so the read port shows the chosen register.

## Construction

- Register file: holds A, X, Y, SP; its read port drives RD.
- Transfer decoder: turns a transfer opcode into the source read select, the destination write
  select, and the write enable.
- Load decode: recognises the load group and forms the destination select from the opcode group.
- Write-data bus: the immediate and the register file read-out each gate onto a shared tri-state
  bus, the immediate enabled on a load and the read-out on a transfer, so exactly one drives the
  register file's write data. The read select is the transfer source on a transfer and RSx
  otherwise, and the write enable is the load-or-transfer decode.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
