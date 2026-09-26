# 6502 Sequential ALU Execute

The clocked accumulator-ALU stage: on each clock it runs one accumulator instruction,
stores the result back into the accumulator register, and commits the affected condition
flags into the P status register. It wraps the combinational execute datapath with the two
registers it reads and writes, closing the loop so the accumulator and carry feed back into
the next instruction.

This is the combinational execute datapath made stateful. It covers the logic and add
group: ORA, AND, EOR and ADC.

## Interface

Inputs (17):
- O0..O7 - the opcode
- M0..M7 - the second operand (memory / immediate)
- CLK    - the clock

Outputs (12):
- A0..A7    - the accumulator register
- N, Z, C, V - the P status register condition flags

## Behaviour

On each clock edge, for an accumulator-ALU opcode:

    ORA:  A = A OR  M
    AND:  A = A AND M
    EOR:  A = A XOR M
    ADC:  A = A + M + C,   C = carry out

The accumulator input to the ALU is the current A register; the carry input for ADC is the
current C flag. The result is written back to A. N and Z are committed for every operation
in the group; C and V are committed only by ADC, so ORA, AND and EOR leave the previous C
and V untouched.

## Construction

- Accumulator: a 74377 octal D register, loaded with the ALU result when the opcode is an
  accumulator-ALU instruction; its output is the ALU's A operand.
- Execute: the combinational datapath (decoder + 8-bit ALU + flag logic). Its A operand is
  the accumulator register output and its carry input is the C flag from the P register.
- P status register: loaded per bit - N and Z on any operation in the group, C and V only on
  ADC - so unaffected flags hold. Its C output feeds back to the execute carry input.

The two register outputs feed back into the execute stage that computes their next values,
so a whole instruction settles and clocks in one step.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
