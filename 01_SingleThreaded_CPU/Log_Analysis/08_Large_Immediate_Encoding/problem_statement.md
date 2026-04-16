# Problem Statement — Scenario 08: Large Immediate Encoding

## Objective

Show how the 12-bit immediate limitation forces programmers to decompose
large constants into multiple instructions, and trace exactly how each step
builds the final 32-bit value.

## Problem Context

The ISA's 12-bit signed immediate field can only hold values in `[-2048, 2047]`.
Many useful constants (memory addresses, large masks, bit-pattern constants)
are outside this range.  There is no 32-bit immediate load instruction.

## Examples Covered

Three different large constants and how they are constructed:

1. **`0x00080000`** — Data segment base address (used in Fibonacci)
   ```
   LOAD_IMM R4, #128 → SHL R4, R4, #12 → R4 = 0x80 × 4096 = 0x00080000
   ```

2. **`0xDEADBEEF`** — Arbitrary 32-bit pattern (for testing)
   ```
   LOAD_IMM R0, #0xDEA >> 12 ... (multi-step)
   ```

3. **`0x00100000`** — One past the end of memory (intentionally one over MEM_SIZE)
   ```
   LOAD_IMM R1, #256 → SHL R1, R1, #12 → R1 = 0x100000
   ```

## Expected Outcome

Each constant is correctly assembled into the final register value using
only valid Imm12 values at each step.  The trace confirms register values
at each intermediate step.

## What to Observe

- The number of instructions required grows with the complexity of the constant.
- Getting intermediate shift amounts right is error-prone.
- A single-instruction 32-bit load would eliminate this overhead entirely.
