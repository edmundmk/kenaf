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

/*
    -- Name Resolution

    Resolve each name that appears in a script.  An unqualified name can:

      - Refer to a global.
      - Refer to a local.
      - Declare a local.
      - Refer to an upval.

    Names not found by name lookup are global references.  Assigning to an
    unqualified global name is an error (but this is not checked in this step).

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


    -- Upvals

    Upvals and implement closures.  A variable captured by a function closure
    is an upval.

    There is an upstack, separate from the call stack.  This stack holds upval
    objects.  Like in Lua, an upval object either references a slot in the
    call stack, or stores its captured value in itself.

    At the end of each block, the upstack is closed down to the same size that
    it had on entry to the block, disconnecting upvals from the call stack.

    When each function closure is created, upval objects are created and pushed
    onto the upstack.  If the variable was already captured by another
    function, there will be an existing upval on the stack.  Upval references
    from the upstack are copied into the function object's list of upvals.
    Inside a function, its upvals are identified by an index into this list.

    Upstack indexes are allocated statically in this name resolution pass.


    -- Super

    If a function has an implicit self parameter, then references to 'super'
    actually mean 'superof( self )'.  This happens even when 'super' is used
    as an upval - the actual upval is 'self' and the child function performs
    'superof( self )'.

*/

#include <memory>
#include <vector>
#include <unordered_map>
#include "ast.h"

namespace kf
{

class resolve_names
{
public:

    resolve_names( source* source, ast_script* ast_script );
    ~resolve_names();

    void resolve();

private:

    enum lookup_context { LOOKUP_NORMAL, LOOKUP_UNPACK, LOOKUP_ASSIGN };

    struct upstack_block
    {
        unsigned block_index;       // Index of block in AST.
        unsigned floor_index;       // Index in upstack which anchors this block.
    };

    struct upstack
    {
        // Function this is the upstack of.
        ast_function* function;
        // Stack of unclosed upstack slots, indexing function locals.
        std::vector< unsigned > upstack_slots;
        // List of blocks which may need their close index updated.
        std::vector< upstack_block > upstack_close;
    };

    struct variable
    {
        unsigned index;             // Index in function's upvals or locals.
        bool is_upval;              // Is this an upval?
        bool implicit_super;        // Use superof when referencing.
        bool after_continue;        // Is this value declared after first continue?
    };

    struct scope
    {
        ast_function* function;  // Function this scope is in.
        unsigned block_index;       // Index of block in AST.
        unsigned node_index;        // Index of loop or function in AST.
        unsigned close_index;       // Upstack index on entry to this scope.
        bool after_continue;        // Are we currently in code that can be skipped by continue?
        bool repeat_until;          // Are we currently in the until part of a loop?
        bool is_function() const;   // Is this the scope of a function?
        bool is_loop() const;       // Is this the scope of a loop?

        // Map of names to variables.
        std::unordered_map< std::string_view, variable > variables;
        // Reference to function upstack.
        std::shared_ptr< struct upstack > upstack;
    };

    void visit( ast_function* f, unsigned index );

    void open_scope( ast_function* f, unsigned block_index, unsigned node_index );
    void declare_implicit_self( ast_function* f );
    void declare( ast_function* f, unsigned index );
    void lookup( ast_function* f, unsigned index, unsigned context );
    void close_scope();

    void insert_upstack( upstack* upstack, size_t scope_index, const variable* variable );
    void close_upstack( upstack* upstack, unsigned block_index, unsigned close_index );

    void debug_print( const upstack* upstack );

    source* _source;
    ast_script* _ast_script;
    std::vector< std::unique_ptr< scope > > _scopes;
};

}

#endif

