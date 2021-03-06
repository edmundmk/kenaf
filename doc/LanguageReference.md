# Kenaf Language Reference

**Kenaf** is a simple scripting language designed to be embedded in C++
applications.

This manual describes the syntax and semantics of the language.


# Basics

## Dynamic Typing

Kenaf is a dynamically typed language.  Any variable can hold any value.  The
type of a value is stored along with the value itself.


## Number Type

There is no separate integer type.  All numbers in Kenaf are IEEE 754
double-precision floating point, giving an effective 53 bits of integer
precision.


## Encoding

Kenaf source files must be encoded in UTF-8.  Spaces, tabs, and newlines are
whitespace.  Newlines consist either of a line feed, a carriage return, or the
two-character sequence comprising a carriage return followed by a line feed.


## Tokenization

Comments can be `/* block comments */` or `-- inline comments`.  Kenaf uses
`//` for integer division, so inline comments are introduced with `--`.

A name is a token matching the regular expression `[A-Za-z0-9][A-Za-z0-9_]*`.

The names `and`, `break`, `continue`, `def`, `do`, `elif`, `else`, `end`,
`for`, `if`, `is`, `not`, `or`, `repeat`, `return`, `then`, `throw`, `until`,
`var`, `while`, and `yield` are keywords with special meaning in the grammar.

Other tokens such as operator symbols and literal values are described in the
following sections.


# Expressions

## Operators

Prefix expressions bind most tightly, and consist of:

  * A `name`, naming local variables or global keys.
  * A key expression `prefix.key`, fetching `key` from an object.
  * An index expression `prefix[ index ]`, fetching `index` from an array or
    table.
  * A function call `prefix( a, b )`, calling a function with arguments.
  * `( parentheses )` surround sub-expressions, with the usual effect on the
    precedence order.

Literal values, described individually in later sections, have lower
precedence than prefix expressions.  To use a literal value in a prefix
expression it must be wrapped in parentheses.

Arithmetic operators, in precedence order:

  * The unary operators `-minus`, `+plus` and bitwise `~bit_not`
  * The multiplicative operators `x * multiply`, `x / division`,
    `integer // division`, and `integer % modulo`
  * The additive operators `x + addition`, `x - subtraction`, and
    `string ~ concatenation`.
  * The bitwise shift operators `left << shift`, `logical >> right_shift`, and
    `arithmetic ~>> right_shift`.
  * Bitwise `x & bit_and`.
  * Bitwise `exclusive ^ bit_or`.
  * Bitwise `inclusive | bit_or`.

Next are the comparision operators, which all have the same precedence:

  * Equality tests `x == equal` and `not != equal`.
  * Inequality tests `less < than`, `less <= equal`, `greater > than`, and
   `greater >= equal`.
  * Prototype tests `x is y` and `x is not y`, which check an object's
    prototype chain.

Then logical operators, in precedence order:

  * Unary `not`, which despite being a unary operator has lower precedence
    than comparisions.  `not a == b` is equivalent to `not ( a == b )`.
  * Logical `and`.  If the first operand tests false, the result is that
    operand, otherwise the result is the second operand.
  * Logical `or`.  If the first operand tests true, the result is that operand,
    otherwise the result is the second operand.

Finally:

  * Conditional expressions `if p then x elif q then y else z end`.

Other kinds of expression such as `yield` expressions, assignment rvals, and
array and table constructors, have lower precedence than any operator.  To
use the result of a constructor in an expression it must be wrapped in
parentheses.


## Tests

The values `null`, `false`, and both positive and negative zero, are said to
*test false*.  A test of one of these values will fail.

All other values test true.


## Integer Division

Integer division is floored division, i.e. `floor( dividend / divisor )`.
Despite the name, the operands do not need to be integers.  The sign of the
result matches the sign of the divisor.

The related modulo operation calculates a modulus equal to
`dividend - ( dividend // divisor ) * divisor`.


## Bitwise Operations

Bitwise operations, including bitwise not, shifts, and bitwise logical
operations operate on 32-bit integer values.  Number operands are converted to
a bit pattern by truncating them to integers, encoding negative numbers using
twos-complement representation, and then considering only the least significant
32 bits.

Some examples:

  * The number `43.214` is truncated to `43`, with the bit pattern
    `...00101011`, with higher bits all clear.
  * The number `-43.214` is truncated to `-43` and represented in two's
    complement as `...11010101`, with higher bits all set.
  * Since the bit pattern consists of only the least significant 32 bits,
    the numbers `3316847572` and `-978119724` both map to the same bit pattern
    `11000101101100110001001111010100`.

The 32-bit result of a bitwise operation is converted back to a number by
treating it as an unsigned integer.


## Chained Comparisions

Sequences of comparision expressions chain together, to give expressions like
`a < b < c` their conventional mathematical meaning.  More specifically,
`result = a < b < c` is equivalent to the following code:

```
var _a, _b = a, b
if _a < _b then
    result = false
else
    result = _b < c
```

Each expression is only evaluated once.  As with the `and` operator, evaluation
of later expressions is skipped if any of the comparisons in the chain test
false.


## Shortcut Evaluation

Chained comparisons, and the logical `and` and `or` perform shortcut
evaluation.  If the result is the first operand, then the following operands
are never evaluated at all - i.e. function calls are skipped.

