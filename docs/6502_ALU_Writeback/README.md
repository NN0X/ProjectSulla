# 6502 ALU Write-Back

The clocked accumulator-ALU stage with the accumulator held in the register file rather than
a standalone register. On each clock it runs one accumulator instruction, writes the result
back into the register file's A slot when the instruction stores a result, and commits the
affected condition flags into the P status register. It is the sequential execute stage with
the register file as the accumulator, so A now lives where the other registers do and both the
write-enable and the flag mask are chosen per instruction.

It covers the full accumulator group for cc = 01: ORA, AND, EOR, ADC, CMP and SBC.

## Interface

Inputs (17):
- O0..O7 - the opcode
- M0..M7 - the second operand (memory / immediate)
- CLK    - the clock

Outputs (12):
- A0..A7    - the accumulator (register file A read port)
- N, Z, C, V - the P status register condition flags

## Behaviour

On each clock edge, for an accumulator-group opcode:

    ORA:  A = A OR  M
    AND:  A = A AND M
    EOR:  A = A XOR M
    ADC:  A = A + M + C,        C = carry out
    SBC:  A = A - M - (1 - C),  C = 1 on no borrow
    CMP:  A - M (result discarded), C = 1 on no borrow

The register file's read port supplies the ALU's A operand. The carry input is the current C
flag for ADC and SBC, and is forced high for CMP so it computes a full A - M regardless of
carry. Each instruction commits a different set of results:

    op          writes A   updates N,Z   updates C   updates V
    ORA/AND/EOR    yes         yes           -           -
    ADC / SBC      yes         yes          yes         yes
    CMP            no          yes          yes          -

Any opcode outside the group leaves the registers and flags unchanged.

## Construction

- Register file: A/X/Y/SP, index 0 = A. The read and write selects are fixed to A (this stage
  reads and writes only the accumulator). The write enable is high for the accumulator group
  except CMP, which updates flags without storing a result. Its A read port is both the ALU's
  A operand and this unit's output.
- Execute: the combinational datapath (decoder + 8-bit ALU + flag logic). Its A operand is the
  register file read port; its carry input is the C flag OR-ed with the CMP decode, so CMP sees
  a carry-in of 1.
- P status register: loaded per bit - N and Z on any group op, C on the arithmetic ops
  (ADC/SBC/CMP), V on ADC and SBC only - so unaffected flags hold.

The control terms come from a small opcode decode: the group detect cc = 01 AND (NOT O7 OR O6)
(which excludes STA/LDA), the subtract detect cc = 01 AND O7 AND O6, split by O5 into CMP and
SBC, and the ADC detect. The register file and P outputs feed back into the execute stage that
computes their next values, so a whole instruction settles and clocks in one step.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
