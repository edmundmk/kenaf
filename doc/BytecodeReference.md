# Kenaf Bytecode Reference

Kenaf scripts are compiled to bytecode that can be executed on a virtual
machine.

This manual describes the design of this virtual machine and the instruction
set that runs on it.


# Virtual Machine

## Values

The virtual machine deals with values.  A value can be one of the following:

  * `null`, `true`, or `false`.
  * A number.  All numbers are IEEE 754 double-precision floating-point.
  * A reference to a UTF-8 string.
  * A reference to an object.

Strings and other objects are allocated from a garbage-collected heap.  Objects
are stored along with their type, which is one of the following:

  * A lookup object, which associates values with string keys.
  * An array, which stores values indexed by integers.  Arrays can be resized.
  * A table, which stores values indexed by key values.
  * An environment record, which is a fixed-length tuple of values.
  * A function, which references a program and a list of environment records.
  * A cothread, containing call frame and execution stacks.


## Programs

The virtual machine runs bytecode programs.  Each program consists of:

  * Bytecode instructions.
  * An array of constant values.
  * An array of selectors, which are references to strings used as keys.
  * An array of references to programs used by function closures.
  * A count of the number of environment records required by the function.
  * A count of the number of parameters expected by the function.
  * A flag indicating if the function has a variable argument parameter.
  * A flag indicating if the function is a generator.


## Cothreads

The virtual machine has a stack of cothreads.  Each cothread can be suspended
and resumed independently of the state of other cothreads.  To achieve this,
each cothread has its own execution and call stack.

The *execution stack* is an array of values.  Each function operates on a bank
of numbered registers.  These registers are actually values on the call stack.
Each call frame establishes a register window on the call stack starting at the
current frame pointer.

The *call stack* is an array of call frames.  As functions are called, new
call frames are pushed onto the call stack, providing a new frame pointer for
the called function.


# Instructions

## Instruction Formats

Each instruction is encoded in 32 bits.  There are three different instruction
formats, which divide the instruction into sub-fields.

          31    24   23    16   15     8   7      0
    AB  [        b |        a |        r |   opcode ]
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

The ABJ format is used for comparison and loop instructions.  In this case, an
instruction encoded in AB format *must* be immediately followed by a `JMP`
instruction encoded in AJ format:

    AB  [        b |        a |        r |   opcode ]
    AJ  [                   j |        - |      JMP ]

The virtual machine treats these instruction pairs as a single operation.

## Data Transfer

    AB  [        - |        a |        r |      MOV ]   MOV r, a
    AB  [        - |        a |        r |      SWP ]   SWP r, a

The `MOV` instruction copies the value from register `a` into register `r`.
The `SWP` instruction swaps the values in the two registers.

## Constant Loading

    AC  [                   c |        r |      LDK ]   LDK r, #c
    AC  [                   c |        r |      LDV ]   LDV r, #c

Loads a constant value into register `r`.  For `LDK`, the constant is loaded
from the program's constant pool, indexed by `c`.  For `LDV`, the constant is
one of the following fixed values, depending on `c`:

  * `0` : `null`.
  * `1` : `false`.
  * `2` : `true`.

## Arithmetic

    AB  [        - |        a |        r |      NEG ]   NEG r, a
    AB  [        - |        a |        r |      POS ]   POS r, a
    AB  [        b |        a |        r |      ADD ]   ADD r, a, b
    AB  [        b |        a |        r |     ADDN ]   ADDN r, a, #b
    AB  [        b |        a |        r |      SUB ]   SUB r, a, b
    AB  [        b |        a |        r |     SUBN ]   SUBN r, a, #b
    AB  [        b |        a |        r |      MUL ]   MUL r, a, b
    AB  [        b |        a |        r |     MULN ]   MULN r, a, #b
    AB  [        b |        a |        r |      DIV ]   DIV r, a, b
    AB  [        b |        a |        r |   INTDIV ]   INTDIV r, a, b
    AB  [        b |        a |        r |      MOD ]   MOD r, a, b

These instructions perform arithmetic.  The first operand is the number in
register `a`.  The second operand, if present, is either the number in register
`b`, or (for `N` variants), a number loaded from the current program's constant
array, indexed by `b`.

The interpreter assumes that constant values references by these instructions
are numbers.  If either register operand is not a number, throws a type error.

The `SUB` and `SUBN` instructions swap their operands, i.e. they perform
`b - a`.  Subtraction of a constant can be can expressed using the `ADDN`
instruction with a negated second operand.

## Logical Not

    AB  [        - |        a |        r |      NOT ]   NOT r, a

Writes either `true` or `false` into register `r` depending on the result of
testing the value in register `a`.  `null`, `false`, `+0.0` and `-0.0` test
false.  All other values test true.

## Jump and Test

    AJ  [                   j |        - |      JMP ]   JMP :addr
    AJ  [                   j |        r |       JT ]   JT r, :addr
    AJ  [                   j |        r |       JF ]   JF r, :addr

Jumps by adding a signed 16-bit displacement `j` to the instruction pointer.
The instruction pointer counts in units of one instruction, and is incremented
after each instruction is read.  Displacements are relative to the index of the
instruction following the jump.

`JT` jumps only if the value in register `r` tests true.  `JF` jumps only if
the value in register `r` tests false.

## Comparisons

    ABJ [        b |        a |        r |      JEQ ][ JMP ]    JEQT/JEQF a, b, :addr
    ABJ [        b |        a |        r |     JEQN ][ JMP ]    JEQTN/JEQFN a, #b, :addr
    ABJ [        b |        a |        r |     JEQS ][ JMP ]    JEQTS/JEQFS a, #b, :addr
    ABJ [        b |        a |        r |      JLT ][ JMP ]    JLTT/JLTF a, b, :addr
    ABJ [        b |        a |        r |     JLTN ][ JMP ]    JLTTN/JLTFN a, #b, :addr
    ABJ [        b |        a |        r |     JGTN ][ JMP ]    JGTTN/JGTFN a, #b, :addr
    ABJ [        b |        a |        r |      JLE ][ JMP ]    JLET/JLEF a, b, :addr
    ABJ [        b |        a |        r |     JLEN ][ JMP ]    JLETN/JLEFN a, #b, :addr
    ABJ [        b |        a |        r |     JGEN ][ JMP ]    JGETN/JGEFN a, #b, :addr

Performs a comparison and the jumps depending on the result.  The first operand
is the value in register `a`.  The second operand is either the value in
register `b`, or a value loaded from the current program's constant array,
indexed by `b`.  `N` variants treat the constant as a number.  `S` variants
treat the constant as a string.

The interpreter assumes that constants are of the correct type.

Each instruction has two mnemonics.  `T` variants are encoded with the `r`
field set to `1`, and branch when the condition is `true`.  `F` variants are
encoded with the `r` field set to `0`, and branch when the condition is
`false`.

`JEQ`, `JEQN`, and `JEQS` check two values for equality.  Two numbers compare
equal using the normal rules for floating-point equality.  Two strings compare
equal if they have the same characters.  Other values compare equal only if
they are the same value.  An equality comparison between two values of
different types always results in `false`.

The remaining instructions perform ordering comparisons.  `JLT` and `JLE`
compare either numbers or strings.  If the operands are of different types,
these instructions throw a type error.  `JLTN`, `JGTN`, `JLEN` and `JGEN`
compare a number in register `r` with a constant number.  If the register
operand is not a number, throws a type error.

String comparisons compare successive pairs of coding units until the reaching
a pair which does not compare equal, or reaching the length of the shortest
string, in which case the shorter string compares less than the longer one.

This set of instructions is sufficient to encode numeric comparisons, even in
the presence of NaNs.

    u == v      JEQT u, v
    u != v      JEQF u, v       -- not
    u < v       JLTT u, v
    u <= v      JLET u, v
    u > v       JLTT v, u       -- swapped
    u >= v      JLET v, u       -- swapped

    u == 1.0    JEQTN u, #1.0
    u != 1.0    JEQFN u, #1.0   -- not
    u < 1.0     JLTTN u, #1.0
    u <= 1.0    JLETN u, #1.0
    u > 1.0     JGTTN u, #1.0
    u >= 1.0    JGETN u, #1.0

    1.0 == u    JEQTN u, #1.0
    1.0 != u    JEQFN u, #1.0   -- not
    1.0 < u     JGTTN u, #1.0   -- swapped
    1.0 <= u    JGETN u, #1.0   -- swapped
    1.0 > u     JLTTN u, #1.0   -- swapped
    1.0 >= u    JLETN u, #1.0   -- swapped

To negate a comparison and branch when the condition is `false`, simply swap
the `r` field.

## Concatenation

    AB  [        b |        a |        r |   CONCAT ]   CONCAT r, a, b
    AB  [        b |        a |        r |  CONCATS ]   CONCATS r, a, #b
    AB  [        b |        a |        r | RCONCATS ]   RCONCATS r, a, #b

Concatenates the string in register `a` with the string in register `b`, and
stores the result in register `r`.  If either operand is not a string, throws
a type error.

`CONCATS` uses a string from the constant pool, indexed by `b` as the second
operand.  `RCONCATS` similarly uses a string from the constant pool as the
second operand, and also swaps the two operands, concatenating the constant
indexed by `b` with the string in register `a`.

The interpreter assumes that constants are valid strings.

## Bitwise Instructions

    AB  [        - |        a |        r |   BITNOT ]   BITNOT r, a
    AB  [        b |        a |        r |   LSHIFT ]   LSHIFT r, a, b
    AB  [        b |        a |        r |   RSHIFT ]   RSHIFT r, a, b
    AB  [        b |        a |        r |   ASHIFT ]   ASHIFT r, a, b
    AB  [        b |        a |        r |   BITAND ]   BITAND r, a, b
    AB  [        b |        a |        r |   BITXOR ]   BITXOR r, a, b
    AB  [        b |        a |        r |    BITOR ]   BITOR r, a, b

Performs a 32-bit bitwise operation on the numbers in registers `a` and `b`,
and writes the result to register `r`.  If either operand is not a number,
throws a type error.

The operations are:

  * `BITNOT` performs bitwise not.  Has only a single operand.
  * `LSHIFT` performs a left shift.
  * `RSHIFT` performs a logical right shift.
  * `ASHIFT` preforms an arithmetic right shift.
  * `BITAND` performs logical conjuction on each pair of bits.
  * `BITXOR` performs exclusive disjunction on each pair of bits.
  * `BITOR` performs logical disjunction on each pair of bits.

Numbers are converted to bit patterns using the following process:

  * The number is truncated towards zero.
  * Ignoring the sign bit, the truncated value of the number is represented
    as an unsigned integer with as many bits as is required.
  * If the number is negative, take the two's-complement of the bit pattern.
    This causes the high bits of the number to become `1`.
  * Use only the low 32 bits of the resulting bit pattern.

After the operation, the resulting bit pattern is converted back to a number by
considering the low 32 bits and treating them as an unsigned integer.  The
result of a bitwise operation is always in the range `0 <= result <
0xFFFFFFFF`.

## Length

    AB  [        - |        a |        r |      LEN ]   LEN r, a

Queries the length of the value in register `a` and writes the result to
register `r`.

  * The length of a string is the number of UTF-8 encoding units.
  * The length of an array is the number of elements.
  * The length of a table is the number of entries.

Any other value throws a type error.

