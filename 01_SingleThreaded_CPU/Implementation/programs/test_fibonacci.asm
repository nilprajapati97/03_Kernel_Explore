; ============================================================================
; test_fibonacci.asm — Compute first 10 Fibonacci numbers
;
; Custom CPU ISA assembler format:
;   MNEMONIC  OPERANDS       ; optional comment
;
; Registers used:
;   R0  — counter (loop index, starts at 10, counts down to 0)
;   R1  — fib(n-2)  (current)
;   R2  — fib(n-1)  (previous)
;   R3  — fib(n)    (next = R1 + R2)
;   R4  — base address of result array in memory
;   R5  — scratch (constant 4 for pointer advance)
;   R6  — constant 1 (loop decrement)
;
; Result layout in memory (MEM_DATA_BASE = 0x00080000):
;   [0x00080000] = fib(0) = 0
;   [0x00080004] = fib(1) = 1
;   [0x00080008] = fib(2) = 1
;   ...
;   [0x00080024] = fib(9) = 34
;
; All instructions assembled to 32-bit little-endian words.
; ============================================================================

; ── Initialise registers ─────────────────────────────────────────────────────
LOAD_IMM    R1, #0          ; fib(n-2) = 0  (F0)
LOAD_IMM    R2, #1          ; fib(n-1) = 1  (F1)
LOAD_IMM    R0, #10         ; loop counter = 10 iterations
LOAD_IMM    R5, #4          ; stride = 4 bytes per slot
LOAD_IMM    R6, #1          ; decrement constant
LOAD_IMM    R4, #0          ; base address low word  (will be patched)

; Load base address 0x00080000 into R4
; LOAD_IMM only carries 12-bit immediate (max ±2047 in signed context)
; Use two-step: R4 = 0x80 then SHL R4, R4, R_shift
; 0x00080000 = 0x80 << 12 = 128 << 12

LOAD_IMM    R4, #128        ; R4 = 128 = 0x80
LOAD_IMM    R7, #12         ; shift amount = 12
SHL         R4, R4, R7      ; R4 = 0x80 << 12 = 0x00080000

; ── Store fib(0) ─────────────────────────────────────────────────────────────
STORE       [R4 + #0], R1   ; MEM[0x00080000] = 0  (fib 0)

; ── Store fib(1) ─────────────────────────────────────────────────────────────
STORE       [R4 + #4], R2   ; MEM[0x00080004] = 1  (fib 1)

; Advance pointer by 8 (past the two already stored)
LOAD_IMM    R8, #8
ADD         R4, R4, R8      ; R4 = 0x00080008

; R0 = 8 remaining (10 total - 2 already stored)
LOAD_IMM    R0, #8

; ── Loop start ───────────────────────────────────────────────────────────────
; Address of LOOP label = MEM_PROG_BASE + (current instruction index * 4)
; The assembler resolves label addresses automatically.

LOOP:
    ADD     R3, R1, R2      ; fib(n) = fib(n-2) + fib(n-1)
    STORE   [R4 + #0], R3   ; MEM[R4] = fib(n)
    MOV     R1, R2           ; fib(n-2) = fib(n-1)
    MOV     R2, R3           ; fib(n-1) = fib(n)
    ADD     R4, R4, R5       ; advance pointer by 4
    SUB     R0, R0, R6       ; counter--
    CMP     R0, R6           ; compare counter with 1
    JGT     LOOP             ; if counter > 1, loop
    ; One final iteration when R0 == 1
    ADD     R3, R1, R2
    STORE   [R4 + #0], R3

; ── HALT ─────────────────────────────────────────────────────────────────────
HALT
