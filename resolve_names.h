//
//  resolve_names.h
//
//  Created by Edmund Kapusniak on 30/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef RESOLVE_NAMES_H
#define RESOLVE_NAMES_H

namespace kf
{

/*
    -- Name Resolution

    Resolve each name that appears in a script.  An unqualified name can:

      - Refer to a global.
      - Refer to a local.
      - Declare a local.
      - Refer to an upval.

    Names not found by name lookup are global references.  It's an error to
    assign to an unqualified global name - code must explicitly use something
    like 'global.new_global', assuming 'global' is provided by the environment.

    The 'until' clause of a repeat until loop has a special rule where it can
    only refer to variables that were declared before the first 'continue' in
    the loop.

    Name resolution modifies the AST in place.  After name resolution, function
    ASTs can be considered independently.


    -- Locals

    Locals are parameters and declared variables.  Locals go out of scope at
    the end of the block in which they are declared.  For indexes are treated
    as if they were declared at the top of the for loop's block.

    Locals are stored in a local list for each function, and are subsequently
    referred to by index.  The first n locals are the function's parameters.


    -- Upvals and Downvals

    Upvals and downvals implement closures.

    A variable is a downval if it is referenced by a child function.  A
    variable is an upval if it is declared in a parent function.

    There is a downval stack, separate from the call stack.  Each downval is
    allocated a slot on this stack when it is declared.  At the end of each
    block that declares at least one downval, the downval stack is popped down
    to some index, closing all downvals declared in the block.

    When a function closure is created, the new function's upval slots are
    filled by copying upval objects from the downval stack into the function
    object.  If a slot in the downval stack is empty, a new upval object is
    created.  In this way, downvals used by multiple functions use the same
    upval object.

    Upval objects use the Lua strategy of referring to the original variable's
    stack slot until they are closed, at which point the value is copied into
    the upval object itself.


    -- Super

    If a function has an implicit self parameter, then references to 'super'
    actually mean 'superof( self )'.  This happens even when 'super' is used
    as a downval - the actual downval is 'self' and the child function performs
    'superof( self )'.

*/



}

#endif

