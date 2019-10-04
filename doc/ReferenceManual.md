# Kenaf

**Kenaf** is a scripting language designed to be embedded in C++ applications.

This manual describes the syntax and semantics of the language.


# Source Files

Kenaf source code is UTF-8 text.

Block comments look like this:

    /* comment */

In Kenaf, the character sequence `//` is used for integer division, making it
unavailable for use in comments.  Instead, line comments look like this:

    -- comment

The parser breaks the source text into a sequence of tokens.  Identifier tokens
are produced from character sequences matching the following regular
expression:

    [A-Za-z_][A-Za-z0-9_]*

The following identifiers are *keywords* with special meaning in the grammar.
Any use of a keyword in a position not permitted by the grammar is an error.

    and         break       continue    def
    do          elif        else        end
    false       for         if          is
    not         null        or          repeat
    return      then        throw       true
    until       var         while       yield

Tokens representing number and string literals are described in the sections
discussing number and string values.


# Values and Types

Kenaf is dynamically typed, and has a prototype-based object model.

Each value has a reference to a *prototype*, an object containing shared
keys for all values with the same prototype.  Each prototype has a reference to
a parent prototype, forming a *prototype chain*.

The built-in object named `object` is at the root of every value's prototype
chain.


## Built-In Types

### `null`

`null` is the absence of a value.  The prototype of `null` is `null`.


### `bool`

There are two boolean constants, `true` and `false`.  The builtin object `bool`
is the prototype for boolean values.

The `if` statement, comparison expressions, and the `and`, `or` and `not`
operators, test the truth of a value.  In Kenaf, all values are true except
`null`, `false`, and zero.  Both positive and negative zero are considered to
be false.


### `number`

A number is stored as IEEE 754 double-precision floating point.  The number
prototype is called `number`.

There is no separate integer type.  As doubles, numbers have 53 bits of integer
precision.

Number literals have one of the following forms:

    0





### `string`


### `array`


### `table`


### `object`


## Statements

## Expressions

## Runtime

