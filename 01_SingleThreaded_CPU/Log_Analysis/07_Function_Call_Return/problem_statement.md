# Problem Statement — Scenario 07: Function Call and Return

## Objective

Demonstrate the CPU's `CALL` / `RET` / `PUSH` / `POP` mechanism by executing
a small program that calls a `multiply_by_3` subroutine twice and returns
results to the caller.

## Program Description

```asm
; Main
LOAD_IMM  R0, #5          ; argument: multiply 5 by 3
CALL      MULTIPLY_BY_3   ; LR = PC+4; PC = MULTIPLY_BY_3
STORE     [R4+0], R0      ; store first result (15)

LOAD_IMM  R0, #7          ; argument: multiply 7 by 3
CALL      MULTIPLY_BY_3
STORE     [R4+4], R0      ; store second result (21)
HALT

; Subroutine: R0 = R0 * 3
MULTIPLY_BY_3:
    PUSH  LR              ; save return address (LR = R14)
    LOAD_IMM  R1, #3
    MUL   R0, R0, R1      ; R0 = R0 * 3
    POP   LR              ; restore return address
    RET                   ; PC = LR
```

## Expected Outcome

- CALL sets LR to the instruction after the call, then jumps to the subroutine.
- PUSH LR saves the return address onto the stack.
- MUL performs multiplication.
- POP LR restores the return address.
- RET jumps back to the caller.
- Results: MEM[base+0] = 15, MEM[base+4] = 21.

## What to Observe

- SP changes: decremented on PUSH, restored on POP.
- LR holds the correct return address before each RET.
- The subroutine is entered and exited cleanly twice.
- Stack is balanced (SP returns to initial value after each call).
