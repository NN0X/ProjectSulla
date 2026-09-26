# 6502 ALU Write-Back

The clocked accumulator-ALU stage with the accumulator held in the register file rather than
a standalone register. On each clock it runs one accumulator instruction, writes the result
back into the register file's A slot, and commits the affected condition flags into the P
status register. It is the sequential execute stage with the register file as the accumulator,
so A now lives where the other registers do and its write is gated by decode.

It covers the logic and add group: ORA, AND, EOR and ADC.

## Interface

Inputs (17):
- O0..O7 - the opcode
- M0..M7 - the second operand (memory / immediate)
- CLK    - the clock

Outputs (12):
- A0..A7    - the accumulator (register file A read port)
- N, Z, C, V - the P status register condition flags

## Behaviour

On each clock edge, for an accumulator-ALU opcode:

    ORA:  A = A OR  M
    AND:  A = A AND M
    EOR:  A = A XOR M
    ADC:  A = A + M + C,   C = carry out

The register file's read port supplies the ALU's A operand; the carry input for ADC is the
current C flag. The result is written back to the A register through the register file's write
port. The write enable is asserted only for an accumulator-ALU opcode, so any other opcode
leaves the registers unchanged. N and Z are committed for every operation in the group; C and
V are committed only by ADC, so ORA, AND and EOR leave the previous C and V untouched.

## Construction

- Register file: A/X/Y/SP, index 0 = A. The read select is fixed to A (this stage reads and
  writes only the accumulator); the write select is fixed to A; the write enable is the
  accumulator-ALU decode. Its A read port is both the ALU's A operand and this unit's output.
- Execute: the combinational datapath (decoder + 8-bit ALU + flag logic). Its A operand is the
  register file read port and its carry input is the C flag from the P register.
- P status register: loaded per bit - N and Z on any operation in the group, C and V only on
  ADC - so unaffected flags hold. Its C output feeds back to the execute carry input.

The register file and P outputs feed back into the execute stage that computes their next
values, so a whole instruction settles and clocks in one step.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
