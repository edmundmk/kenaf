# Kenaf Language Reference

**Kenaf** is a simple scripting language designed to be embedded in C++
applications.

This manual describes the syntax and semantics of the language.


# Basics

## Values

Kenaf is a dynamically typed language.  Any variable can hold any value.  The
type of a value is stored along with the value itself.

There is no separate integer type.  All numbers in Kenaf are IEEE 754
double-precision floating point, giving an effective 53 bits of integer
precision.


## Source

Kenaf source files must be encoded in UTF-8.  Spaces, tabs, and newlines are
whitespace.  Newlines consist either of a line feed, a carriage return, or the
two-character sequence comprising a carriage return followed by a line feed.

Comments can be `/* block comments */` or `-- inline comments`.  Kenaf uses
`//` for integer division, so inline comments are introduced with `--`.

A name is a token matching the regular expression `[A-Za-z0-9][A-Za-z0-9_]*`.

The names `and`, `break`, `continue`, `def`, `do`, `elif`, `else`, `end`,
`for`, `if`, `is`, `not`, `or`, `repeat`, `return`, `then`, `throw`, `until`,
`var`, `while`, and `yield` are *keywords* with special meaning in the grammar.


# Expressions

## Precedence

Prefix expressions bind most tightly, and consist of:

  * A `name`, naming local variables or global keys.
  * A key expression `prefix.key`, fetching `key` from an object.
  * An index expression `prefix[ index ]`, fetching `index` from an array or
    table.
  * A function call `prefix( a, b )`, calling a function with arguments.
  * `( parentheses )` surround sub-expressions, with the usual effect on the
    precedence order.

Literal values, described individually in later sections, represent constant
values.  To use a literal value in a prefix expression, you must wrap it in
parentheses, though this is not very useful.

Next in precedence order are:

  * The unary operators `-minus`, `+plus` and bitwise `~not`
  * The multiplicative operators `x * multiply`, `x / division`, `integer // division`, and `integer % modulo`
  * The additive operators `x + addition`, `x - subtraction`, and
    `string ~ concatenation`.
  * The bitwise shift operators `left << shift`, `logical >> right_shift`, and
    `arithmetic ~>> right_shift`.
  * Bitwise `x & and`.
  * Bitwise `exclusive % or`.
  * Bitwise `inclusive | or`.

Comparison operators `x == equal`, `not != equal`, `less < than`,
`less <= equal`, `greater > than`, and `greater >= equal` have lower precedence
than bitwise logical operators.  The expresions `x is y` and `x is not y`,
which test an object's prototype chain, are also comparison operators.




## Integer Division

Integer division is floored division, i.e. `floor( dividend / divisor )`.
Despite the name, the operands do not need to be integers.  The sign of the
result matches the sign of the *divisor*.

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
  * The number `-43` is represented in two's complement as `...11010101`, with
    higher bits all set.
  * Since the bit pattern consists of only the least significant 32 bits,
    the numbers `3316847572` and `-978119724` both map to the same bit pattern
    `11000101101100110001001111010100`.

The 32-bit result of a bitwise operation is converted back to a number by
treating it as an unsigned integer.


## Chained Comparisions

Sequences of comparision expressions chain together, to give expressions like
`a < b < c` their conventional mathematical meaning.  More formally,
`a < b < c` is equivalent to the following code:

```
var result
var temp_a, temp_b = a, b
if temp_a < temp_b then
    result = false
else
    result = temp_b < c
```

Each expression is only evaluated once.  As with the `and` operator, evaluation
of later expressions is skipped if any of the comparisons in the chain is
false.

