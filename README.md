# Kenaf

Kenaf is a small scripting language designed to be embedded in C++
applications.

Kenaf is very similar to [Lua](https://lua.org), except:

  * Arrays, hash tables, and objects are distinct types.
  * Array indexes start from zero.
  * Instead of metatables, a prototype-based object model is built-in.
  * Garbage collection runs concurrently.

I wrote it because I love writing languages, and because these kinds of small,
dynamically-typed language have been so helpful for so many applications.
Kenaf is my latest attempt to make something useful.

## Syntax

Here's a quick overview of Kenaf's syntax:

    -- line comment
    /* block comment */

    -- variable declaration
    var declared
    var variable = "Hello, World!"
    var a, b = 3, 4

    -- assignment
    variable = 4
    a, b = b, a

    -- if statement
    if x < y then
        statement()
    elif x < z then
        statement()
    else
        throw error_object()
    end

    -- conditional expression
    variable = if x < y then 4 else 3 end

    -- block
    do statement() statement() end

    -- loops
    while x < y do statement() end
    repeat statement() until x >= y
    for i = start : limit : step do process( i ) end
    for x : iterable do process( x ) end

    -- break and continue
    while true do
       if x < y then break end
       if x > y then continue end
    end

    -- function definition
    def function( a, b )
        return a + b
    end

    -- function literal
    var f = def( a, b ) return a + b end

    -- object definition
    def object : prototype

        -- object keys
        a_key : "Frobz"
        b_key : def( x ) function( x, x ) end

        -- object defined and assigned to key on outer object
        def member_object
            c_key : 4
        end

        -- method definition
        def method()
            print( self.a_key )
        end

    end

    -- out-of-line method definition
    def object.b_method()
        return self.member_object
    end

##  Scope

Kenaf is lexically scoped.  Variables are declared with the `var` keyword.

Name resolution binds an identifier to a variable in one of the following
scopes:

  * The current block.
  * An outer block in the same function.
  * An outer function.  The function becomes a closure, and the variable is
    captured.  Variables are captured by reference.
  * A global, if no declaration is found.  Global names can only be read.
    Assigning to a global name is an error.

A block consists of a set of statements between `do`..`end` or `then`..`end`.
A `repeat`..`until` loop also introduces a scope, which extends to the end of
the `until` expression.  Variables declared before the first `continue` may be
used in the loop condition.

The variables named in a `for` loop are declarations.  These variables are
valid only in the scope of the loop.

You can assign a new key to the `global` object.  This is the only way to add a
name to the global scope.

## Types

Kenaf is dynamically typed.  Each value has one of the following types:

    def end                   -- object
    null                      -- null
    true                      -- bool
    false                     -- bool
    32.43                     -- number
    "Hello, World"            -- string
    [ 3, 4, 5 ]               -- array
    [ "k" : 33, 44 : "v" ]    -- table (empty table is [:])
    def() return 3 end        -- function
    generator()               -- cothread

Kenaf is prototype-based.  Each value has a prototype, an object to which key
lookups are delegated.  There is a built-in prototype object for each type.
The ultimate prototype of all values is `object`.

Numbers are IEEE 754 double precision floating-point.  This gives 53 bits of
integer precision.  Integer division, expressed with the`//` operator, is
floored division.

Strings are concatenated with the binary `~` operator.

## Objects

Values of object type map keys to values.  Object keys must be strings.  Unlike
values of other types, objects explicitly store a reference to their prototype,
which may be any object.

A lookup expression `obj.key` refers to the value associated with `key` in
`obj`.  If `obj` has no entry for `key`, or if `obj` is not itself an object,
then the lookup is delegated to the value's prototype object.

Assignments always modify `obj`, adding new entries as needed.  In this way,
objects can override the keys of their prototype.

Once an object is used as a prototype, it is *sealed*.  Values may be modified,
but no new keys may be added to a sealed object.

## Methods

A function defined in an object, or a function defined using a qualified name
is a *method*.

    def obj
        def method_one() return 4 end
    end

    def obj.method_two()
        return 5
    end

Methods are given an implicit first parameter called `self`.

When the result of a lookup is called, the object that was used for the lookup
is passed as an implicit first argument to the function call:

    obj.method_one( 4 )
    -- becomes:
    var method = obj.method_one
    method( obj, 4 )

In this way, methods implicitly get a reference to the object they were called
on.

The programmer must be aware of `self` when storing method values outside of an
object, or storing non-method functions in objects.

A method also has a hidden reference to the object it was defined in.  The
special name `super` can be used to refer to this object's prototype.  This
allows methods to access overridden keys from the prototype.

## Comparisons

Comparison operators chain in the same way they do in
[Python](https://www.python.org):

    if 0 <= x < 4 and 0 <= y < 8 then
        print( "coordinate in range" )
    end

Unlike in C, bitwise and `&`, exclusive-or `^`, and or `|` have lower
precedence than the comparison operators.

## Unpacking

Kenaf allows lists of multiple values in the following situations:

  * On the right side of an assignment statements.
  * The arguments passed to a function.
  * The element values in an array construction expression.
  * In a return statement.  Function calls can return multiple values.

The last value in such a list can be *unpacked*.  This is a function call,
array value, or variable argument list which is expanded to multiple values.

    -- this function takes a variable number of extra arguments
    def func( a, arguments ... )

        -- construct an array containing the variable argument list
        var b = [ arguments ... ]

        -- return multiple values
        return a, b
    end

    -- get both results from function call
    var p, q = func( 34, "a", "b", "c" ) ...
        -- p gets the value 34
        -- q gets the value [ "a", "b", "c" ]

    -- unpack elements from array
    var x, y, z = q ...
        -- x gets the value "a"
        -- y gets the value "b"
        -- z gets the value "c"

The use of the ellipsis `...` makes the use of multiple values explicit in the
source code.  In its absence, the result of a function call is truncated to
just the first value.

## Generators and Coroutines

A function defined with the `yield` keyword is a *generator*.

    def yield generator( a, b )
        yield a
        var c = yield b
        yield c + 1
    end

Calling a generator does not pass control to the function.  Instead, a cothread
value is created.  A cothread (i.e. a fiber, coroutine, or green thread) has
its own call stack.

When a cothread is created, the arguments passed to the generator function are
copied onto the cothread's stack.  The cothread is then immediately suspended
at the start of the generator, as if by a `yield` with no results and expecting
no values.

Each time a cothread is called, the arguments become the results of the
previous `yield`.  Each time a cothread yields, the values from the `yield`
become the result of the call that resumed the cothread.

    var g = generator( 1, 2 )
    print( g() )    -- prints 1
    print( g() )    -- prints 2
    print( g( 4 ) ) -- prints 5

A `for` loop can be used to enumerate all values returned by a generator:

    def yield fib( x )
        var a, b = 0, 1
        for i = 0 : x do
            yield a
            a, b = b, a + b
        end
    end

    for n : fib( 10 ) do print( n ) end

A `yield` statement is only valid in the body of a generator function.  A
generator can call another generator as if it was a normal function using
`yield for`.  In this case no new cothread is constructed.  The nested call
yields the existing cothread.

## Exceptions

Kenaf has a `throw` statement, which will throw an exception value.

However, there is no way to *catch* an exception in Kenaf.  The intention is
that Kenaf will be used to add functionality to a host program.  If an error
occurs in a script then it can be caught in C++ code and handled there.

## Virtual Machine

The virtual machine is a register-based interpreter.  The compiler attempts to
produce the best bytecode it can, by performing a few optimization passes:

  * Constant folding.
  * Dead code elimination.
  * Register allocation, attempting to minimize VM stack usage and eliminate
    redundant move instructions.

Kenaf is garbage collected.  The garbage collector uses a basic mark-sweep
algorithm.  However, both the mark and sweep phases run concurrently, on
another thread, and do not block further execution of script code.  This design
attempts to minimize pause times and increase Kenaf's suitability for use in
interactive applications.

## License

Copyright © 2019 Edmund Kapusniak.  Licensed under the MIT License.  See the
LICENSE file in the project root for full license information.
