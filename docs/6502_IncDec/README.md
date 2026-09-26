# 6502 Increment / Decrement

A one-step increment or decrement of a byte, with the N and Z flags. It is the datapath core
for INC and DEC on memory and for the index-register forms INX, DEX, INY and DEY.

## Interface

Inputs (9):
- D0..D7 - the value
- DEC    - direction: 0 = increment, 1 = decrement

Outputs (10):
- R0..R7 - the result
- N      - negative: result bit 7
- Z      - zero: result is 0x00

## Behaviour

    increment (DEC = 0): R = D + 1
    decrement (DEC = 1): R = D - 1

Both wrap in eight bits. N is the top bit of the result and Z is set when the result is zero.
The carry flag is not affected by these instructions, so no carry is produced.

## Construction

The step is the 8-bit ALU (the 74181 pair) in add mode. The second operand and the carry-in are
both tied to the direction line, which turns one add into either sense: with DEC low the operand
is 0x00 and the carry-in adds one, giving D + 1; with DEC high the operand is 0xFF and there is
no carry-in, giving D + 0xFF = D - 1. N is the result's top bit and Z is a NOR across the result
byte.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
