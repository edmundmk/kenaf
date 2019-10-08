
# Rationale

## Why another scripting language?

I needed a scripting language, I like writing parsers and compilers, and I am
seemingly cursed to build everything myself from scratch.

## Why does Kenaf look so much like Lua?

Kenaf owes a *lot* to Lua.  Lua is a great language, with only a few rough
edges.  Lua source code is also extremely readable, and I needed something that
could be understood by as many readers as possible.

## Why pick `/* */` for block comments but `--` for inline comments?

Several languages use `--` for comments, including SQL.

Using `//` for inline comments means picking another symbol for floored
division, and I couldn't find a good alternative.

The backslash `\` has a fixed role as an escape character in most contexts,
including in strings in Kenaf itself. Two-character sequences like `-/` or
similar require more effort to type than just hitting the same key twice.  In a
lot of domains, integer division is going to be more common than floating point
division.

The current version of Kenaf doesn't have multiline strings, so the Lua or
Python approach of using string syntax for block comments doesn't work either.

## Why pick `~` for string concatentation?

Having a separate operator for string concatenation reduces the ambiguity
of the operation performed by `+`.  I considered this to be important not only
to help avoid mistakes, but also to give potential future JIT or AOT compilers
less to deal with.

Once I'd ruled out `+`, reusing `~` seemed like a good choice.

## Why isn't there an integer type?

JavaScript seems to have proven that we don't need one.  53-bits is probably
more than enough for most calculations in most situations where Kenaf will be
used.

Lack of a 64-bit integer does present a problem when interacting with an API
which returns 64-bit integers as handles or as ID values.  These kind of values
will have to be exposed to Kenaf as some other kind of object.

## Why do bitwise operations truncate to 32 bits?

I looked at the way JavaScript handles the same operations.

## Why does `>>` perform logical right shift?  Why is arithmetic right shift `~>>`?

Since bare `<<` performs *logical* left shift, I feel like the mirrored
operator `>>` should also perform a logical shift.

I don't have any evidence, but I suspect that now we all have hardware
multipliers bitwise operations are much more common on unsigned types.  So when
C programmers type `>>` they are asking for a logical shift most of the time.
It makes sense to have to shorter token represent the more common operation.

Given that, using `>>>` for arithmetic right shift would invite confusion with
Java and JavaScript's shift operators, which use the shorter form for the
*arithmetic* shift.  So we needed another token.  I felt like `~`, thanks to its
use in bitwise not, can represent the sign bit being shifted in on the left.

The tilde also appears to be my favourite character when I need a new operator
symbol.

## Why is there no operator overloading?

I have no philosophical objection to operator overloading.  I find it very
useful when the operators make sense for the type.

There are, however, good reasons not to add it to Kenaf.  First, the simpler
the language and the fewer features, the fewer things that can break.  Kenaf is
deliberately much simpler than previous languages I've designed.

Second, allowing operator overloading increases the number of valid
combinations of types on either side of an arithmetic operator.  Without
operator overloading, a theoretical JIT or AOT compiler can potentially find
more situations where it can infer types and avoid dynamic dispatch for
arithmetic expressions.

Lua's metatables are incredibly powerful, but the first thing everyone does
with them is implement an object mode.  Kenaf has one of those built-in
already.

## Why is list unpacking explicit with `...`?  Why have unpacking and not full destructuring?



## Why does Kenaf have generators and coroutines?

This is another feature inherited from Lua.  I needed Kenaf to be 'stackless'
by default, allowing calls to be be suspended by native code until an answer is
available.  Given that native code can do this, why not allow scripts to use
the same machinery?

## Why isn't Kenaf statically typed?

Implementing a large project in a dynamic language without type checking is
a terrible idea.

But Kenaf is not designed for writing large projects.  It is designed to be
both embeddable and readable, for situations where you need relatively small
bits of logic that can be read and updated by the widest possible audience.

Type annotations add noise to small scripts, making them less readable for
people new to a language.  Type systems are also complex to implement.

