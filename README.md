# Kenaf

**Kenaf** is a scripting language designed to be embedded in C++ applications.
Kenaf provides you with:

  * Simple, readable syntax.
  * A built-in object model based on prototypes.
  * An interpreter with decent performance.
  * Interaction with native C++ code.
  * A concurrent garbage collector that minimizes stalls.

Kenaf owes a great deal to [Lua](https://lua.org) and
[Python](https://www.python.orgpython.org).  These languages get an awful lot
right, and where they do so, Kenaf follows their lead.

Since Kenaf is most similar to Lua, it is useful for readers familiar with Lua
to illustrate some of the differences between the two languages:

  * Kenaf makes a distinction between objects, arrays, and tables.
  * In Kenaf, array indexes start at zero.
  * Conditional operators chain in the same way as in Python.
  * Kenaf does not support operator overloading or other metatable hooks.
  * The most common use of metatables - to provide inheritance - is built-in.

Kenaf was designed for situations where you want to treat code as data.  Using
Kenaf, you can make your application scriptable or your game moddable.

