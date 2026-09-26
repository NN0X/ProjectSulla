# 6502 ALU Control Decoder

Decodes a 6502 opcode into the control lines that drive the 8-bit ALU (the 74181 pair)
for the accumulator-group instructions. In the 6502 opcode layout aaabbbcc, the group
cc = 01 holds the arithmetic/logic instructions, selected by the aaa field:

    aaa  instruction   ALU action
    000  ORA           A OR  M
    001  AND           A AND M
    010  EOR           A XOR M
    011  ADC           A +   M
    100  STA           (store - no ALU op)
    101  LDA           (load  - no ALU op)
    110  CMP           A -   M   (result discarded, flags only)
    111  SBC           A -   M

## Interface

Inputs (8):
- O0..O7 - the opcode

Outputs (6):
- S0..S3 - 74181 function select
- ALUM   - 74181 mode (1 = logic, 0 = arithmetic)
- ISALU  - high when the opcode is one of the six accumulator-group ALU operations

For opcodes outside cc = 01, and for STA/LDA (which do not use the ALU), every output is
low.

## Decoded control words

The 74181 select/mode for each operation (established by searching the 74181 function for
the code that computes each operation):

    op    S3 S2 S1 S0   ALUM
    ORA    1  1  1  0    1     (logic OR)
    AND    1  0  1  1    1     (logic AND)
    EOR    0  1  1  0    1     (logic XOR)
    ADC    1  0  0  1    0     (A plus M)
    CMP    0  1  1  0    0     (A minus M)
    SBC    0  1  1  0    0     (A minus M)

Carry handling (the ALU carry-in: 1 for add, 0 for subtract, and the carry flag for
ADC/SBC) is applied by the control layer, not by this decoder.

## Construction

A two-level gate PLA. The AND plane forms one product term per operation: the group
detector (O0 high, O1 low) AND-ed with the three aaa literals for that operation. The OR
plane sums, for each output line, the product terms of the operations that assert it.

## Reference

6502 opcode matrix: https://www.masswerk.at/6502/6502_instruction_set.html
