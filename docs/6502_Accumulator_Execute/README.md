# 6502 Accumulator Datapath

The full clocked accumulator stage. On each clock it runs one accumulator instruction - either
an ALU-group operation or an accumulator shift - writes the result back into the register file's
A slot when the instruction stores one, and commits the affected flags into the P status
register. It is the ALU write-back stage widened with the shifter, so the accumulator's next
value is selected from whichever unit the opcode calls for.

Instruction groups covered:

    cc = 01   ORA  AND  EOR  ADC  CMP  SBC        (via the ALU execute datapath)
    cc = 10   ASL  ROL  LSR  ROR   (accumulator)  (via the shifter)

## Interface

Inputs (17):
- O0..O7 - the opcode
- M0..M7 - the second operand for the ALU group (unused by the shifts)
- CLK    - the clock
- EN     - write enable: the result and flags are committed only on a clock where EN is high

Outputs (12):
- A0..A7    - the accumulator (register file A read port)
- N, Z, C, V - the P status register condition flags

## Behaviour

The register file's A read port drives both the ALU execute block and the shifter. The opcode
group picks which result is written back and which flags it produces:

    ALU group (cc = 01): result, N, Z, and C/V as the ALU execute stage defines them
    shift group (cc = 10): the shifted byte, its top bit as N, zero-detect as Z, the
                           bit shifted out as C; V is not affected

The carry flag feeds the ALU carry-in (forced high for CMP) and the shifter's rotate input, so
ROL and ROR rotate through the same carry that ADC and SBC use. A whole instruction settles and
clocks in one step.

Which flags each instruction commits, and whether it writes the accumulator:

    group / op        writes A   N,Z   C            V
    ORA/AND/EOR         yes       yes   -            -
    ADC/SBC             yes       yes   yes          yes
    CMP                 no        yes   yes          -
    ASL/ROL/LSR/ROR     yes       yes   yes (out)    -

Any opcode outside these groups leaves the registers and flags unchanged. EN gates every write, so with EN low a clock changes nothing; the control unit raises EN on the cycle an instruction commits its result.

## Construction

- Register file: A/X/Y/SP, index 0 = A, read and write selects fixed to A.
- ALU execute: the cc = 01 datapath (decoder + 8-bit ALU + flag logic).
- Shifter: the single-place shift/rotate unit; its direction and fill come from opcode bits
  O6 and O5, its data from the A read port, its carry-in from the C flag.
- Result mux: a shift-group detect (cc = 10 with the top aaa bit clear) selects the shifter's
  byte and flags over the ALU's; a small zero-detect NORs the shifted byte for its Z.
- P status register: the write-enable and per-flag load masks are formed from the ALU-group and
  shift-group decodes - N and Z on any accumulator op, C on the arithmetic ops and the shifts,
  V on ADC and SBC only; the accumulator write-enable is every storing op (the ALU group except
  CMP, plus the shifts).

The register file and P outputs feed back into the execute and shift blocks that compute their
next values.

## Reference

6502 instruction set: https://www.masswerk.at/6502/6502_instruction_set.html
