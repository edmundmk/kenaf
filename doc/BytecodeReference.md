# Kenaf Bytecode Reference

**Kenaf** scripts are compiled to bytecode that can be executed on a virtual
machine.

This manual describes the design of this virtual machine and the instruction
set that runs on it.


# Virtual Machine


# Instructions

## Instruction Formats

Each instruction is encoded in 32 bits.  There are four different instruction
formats, which divide the instruction into sub-fields.

          31    24   23    16   15     8   7      0
    AB  [        b |        a |        r |   opcode ]
    AI  [        i |        a |        r |   opcode ]
    AC  [                   c |        r |   opcode ]
    AJ  [                   j |        r |   opcode ]

The low byte of each instruction contains the opcode.  The remaining fields
encode one of the following:

  * `r`, `a`, and `b` are unsigned 8-bit register numbers, indexes, or
    immediates.
  * `c` is an unsigned 16-bit index or immediate.
  * `i` is a signed 8-bit immediate.
  * `j` is a signed 16-bit jump offset or immediate.

Jump offsets are encoded as signed displacements relative to the instruction
following the jump, counting in instructions.


## Data Transfer

### MOV

    AB  [        - |        a |        r |      MOV ]   MOV r, a

Copies the value from register `a` into register `r`.

### SWP

    AB  [        - |        a |        r |      SWP ]   SWP r, a

Swaps the values in registers `r` and `a`.

## Constant Loading

### NULL

    AB  [        - |        - |        r |     NULL ]   NULL r

Loads `null` into register `r`.

### BOOL

    AC  [                   c |        r |     BOOL ]   BOOL r, [c]

Loads a boolean value into register `r`.  If `c` is zero, the value is `false`,
otherwise the value is `true`.

### LOADK

    AC  [                   c |        r |    LOADK ]   LOADK r, [c]

Loads a value from the function's constant pool, indexed by `c`, into the
destination register `r`.

## Arithmetic

### LENGTH

    AB  [        - |        a |        r |   LENGTH ]   LENGTH r, a

Queries the length of the value in register `a` and writes the result to
register `r`.

  * The length of a string is the number of UTF-8 encoding units.
  * The length of an array is the number of elements.
  * The length of a table is the number of entries.

Any other value throws a `type_error`.

### NEG

    AB  [        - |        a |        r |      NEG ]   NEG r, a

Calculates the additive inverse of the number in register `a` and writes the
result to register `r`.  If the value is not a number, throws a `type_error`.

### POS

    AB  [        - |        a |        r |      POS ]   POS r, a

Performs unary plus.  Takes the number in register `a` and writes it to
register `r`.   If the value is not a number, throws a `type_error`.

### BITNOT

    AB  [        - |        a |        r |   BITNOT ]   BITNOT r, a

Calculates the one's complement of the number (treated as a 32-bit integer)
in register `a` and writes the result to register `r`.  If the value is not a
number, throws `type_error`.

### NOT

    AB  [        - |        a |        r |      NOT ]   NOT r, a

Writes the logical negation of the value in register `a` to register `r`.  If
the value tests true, the result is `false`, otherwise the result is `true`.

### ADD

    AB  [        b |        a |        r |      ADD ]   ADD r, a, b
    AB  [        b |        a |        r |     ADDK ]   ADDK r, a, [b]
    AI  [        i |        a |        r |     ADDI ]   ADDI r, a, [i]

There are three forms of the `ADD` instruction, which differ in the treatment
of the second operand.  `ADDK` uses a value from the constant pool, indexed by
`b`.  `ADDI` uses the 8-bit signed integer immediate `i`.

Adds the number in register `a` to the second operand and writes the result to
register `r`.  If either operand is not a number, throws `type_error`.

### SUB

    AB  [        b |        a |        r |      SUB ]   SUB r, a, b
    AB  [        b |        a |        r |     SUBK ]   SUBK r, a, [b]
    AI  [        i |        a |        r |     SUBI ]   SUBI r, a, [i]

Performs a subtraction, with the same forms as the `ADD` instruction.  The
order of the operands is swapped - the number in register `a` is subtracted
from the second operand.

### MUL

    AB  [        b |        a |        r |      MUL ]   MUL r, a, b
    AB  [        b |        a |        r |     MULK ]   MULK r, a, [b]
    AI  [        i |        a |        r |     MULI ]   MULI r, a, [i]

Performs a multiplication, with the same forms as the `ADD` instruction.

