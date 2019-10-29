# Kenaf Bytecode Reference

**Kenaf** scripts are compiled to bytecode that can be executed on a virtual
machine.

This manual describes the design of this virtual machine and the instruction
set that runs on it.


# Virtual Machine

## Values

The virtual machine deals with values.  A value can be one of the following:

  * `null`
  * `true`
  * `false`
  * A number.  All numbers are IEEE 754 double-precision floating-point.
  * A reference to a UTF-8 string, stored with its length.  Strings are not
    null terminated.
  * A reference to an object.

Strings and other objects are allocated from a garbage-collected heap.  Objects
are stored along with their type, which is one of the following:

  * A lookup object, which associates values with string keys.
  * An array, which stores values indexed by integers.  Arrays can be resized.
  * A table, which associates values with keys of other values.
  * An environment record, which is a fixed-length tuple of values.
  * A function closure, which references a program and environment records.
  * A cothread.


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

### LDV

    AB  [                   c |        r |      LDV ]   LDV r, #c

Loads a value into register `r` based on `c`:

  * 0 : `null`.
  * 1 : `false`.
  * 2 : `true`.

### LDK

    AC  [                   c |        r |      LDK ]   LDK r, #c

Loads a value from the function's constant pool, indexed by `c`, into the
destination register `r`.

## Arithmetic

### LEN

    AB  [        - |        a |        r |      LEN ]   LEN r, a

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
    AB  [        b |        a |        r |     ADDN ]   ADDN r, a, #b

There are three forms of the `ADD` instruction, which differ in the treatment
of the second operand.  `ADDN` uses a number from the constant pool, indexed by
`b`.

Adds the number in register `a` to the second operand and writes the result to
register `r`.  If either operand is not a number, throws `type_error`.

### SUB

    AB  [        b |        a |        r |      SUB ]   SUB r, a, b
    AB  [        b |        a |        r |     SUBN ]   SUBN r, a, #b

Performs a subtraction, with the same forms as the `ADD` instruction.  The
order of the operands is swapped - the number in register `a` is subtracted
from the second operand.

### MUL

    AB  [        b |        a |        r |      MUL ]   MUL r, a, b
    AB  [        b |        a |        r |     MULN ]   MULN r, a, #b

Performs a multiplication, with the same forms as the `ADD` instruction.

### CONCAT

    AB  [        b |        a |        r |   CONCAT ]   CONCAT r, a, b
    AB  [        b |        a |        r |  CONCATS ]   CONCATS r, a, #b
    AB  [        b |        a |        r | RCONCATS ]   RCONCATS r, a, #b

Concatenates the string in register `a` with the string in register `b`, and
stores the result in register `r`.  If either operand is not a string, throws
`type_error`.

`CONCATS` uses a string from the constant pool, indexed by `b` as the second
operand.  `RCONCATS` also uses one operand from a register, and one from the
constant pool, but the order in which the strings are concatenated is swapped.

### DIV

    AB  [        b |        a |        r |      DIV ]   DIV r, a, b

Performs a division between the number in register `a` and the number in
register `b`, and stores the result in register `r`.

### INTDIV

    AB [         b |        a |        r |   INTDIV ]   INTDIV r, a, b

Performs a floored division `⌊a÷b⌋` between the number in register `a` and the
number in register `b`, and stores the result in register `r`.

### MOD

    AB  [        b |        a |        r |      MOD ]   MOD r, a, b

