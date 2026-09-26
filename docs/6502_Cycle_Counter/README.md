# 6502 Cycle Counter

The instruction timing counter for the control unit. It tracks which cycle of the current
instruction is running, counting up once per clock and returning to the fetch cycle when the
instruction finishes or on reset. The rest of the control logic uses this cycle number to decide
what each part of the datapath does on a given clock.

## Interface

Inputs (3):
- RST  - reset: force the count back to the fetch cycle
- DONE - the current cycle is the instruction's last: restart at the fetch cycle next clock
- CLK  - the clock

Outputs (4):
- T0, T1, T2 - the cycle number, 0 to 7 (T0 is the low bit)
- FETCH      - high on cycle 0, the opcode-fetch cycle

## Behaviour

On each clock edge:

    next = 0            when RST or DONE
    next = (T + 1) mod 8   otherwise

So an instruction runs cycle 0, 1, 2, ... until it asserts DONE, after which the next clock
returns to cycle 0 to fetch the next opcode. FETCH marks that cycle-0 fetch.

## Construction

The cycle number is three bits of a 74377 register. Its next value is a three-bit increment of
its current value (bit 0 toggles, bit 1 flips on a carry from bit 0, bit 2 flips on a carry from
bits 1 and 0), forced to zero when RST or DONE is high. FETCH is a NOR of the three state bits.

## Reference

MOS 6502 timing: https://www.masswerk.at/6502/6502_instruction_set.html
