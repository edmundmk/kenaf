//
//  ast_resolve.h
//
//  Created by Edmund Kapusniak on 30/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef AST_RESOLVE_H
#define AST_RESOLVE_H

/*
    -- Name Resolution

    Resolve each name that appears in a script.  An unqualified name can:

      - Declare a local.
      - Refer to a local (either a normal local, or one in a varenv).
      - Refer to a value in an outenv.
      - Refer to a global.

    Names not found by name lookup are global references.  Assigning to an
    unqualified global name is an error.

    The 'until' clause of a repeat until loop has a special rule where it can
    only refer to variables that were declared before the first 'continue' in
    the loop.

    Name resolution modifies the AST in place.  After name resolution, the AST
    of each function in the script can be considered independently.


    -- Locals

    Locals are parameters and declared variables.  Locals go out of scope at
    the end of the block in which they are declared.  For indexes are treated
    as if the entire loop was wrapped in an invisible block.

    Locals are stored in a local list for each function, and are subsequently
    referred to by index.  The first n locals are the function's parameters.


    -- Environment Records

    Environment records implement closures.  All variables which are captured
    by inner functions are stored in environment records.

    Each block with captured variables has an associated hidden local variable,
    called a 'varenv'.  On entry to the block, an environment record is
    created.  Locals captured by inner functions are assigned slots in this
    environment record, and accesses are routed to it.

    When function closures are created, the function's outenv slots are
    populated with environment records.  Accesses to variables in outer
    scopes are routed through these records.


    -- Super

    If a function has an implicit self parameter, then references to 'super'
    actually mean 'superof( self )'.  This magic variable cannot be captured.

*/

#include <memory>
#include <vector>
#include <unordered_map>
#include "ast.h"

namespace kf
{

class ast_resolve
{
public:

    ast_resolve( source* source, ast_script* ast_script );
    ~ast_resolve();

    void resolve();

private:

    enum lookup_context { LOOKUP_NORMAL, LOOKUP_UNPACK, LOOKUP_ASSIGN };

    struct variable
    {
        unsigned index;             // Index of local or outenv.
        bool after_continue;        // Is this value declared after first continue?
        bool implicit_super;        // Use superof when referencing.
        bool is_outenv;             // Is this located in outenv?
        uint8_t outenv_slot;        // Slot in outenv environment record.
    };

    struct scope
    {
        ast_function* function;     // Function this scope is in.
        unsigned block_index;       // Index of block in AST.
        unsigned node_index;        // Index of loop or function in AST.
        unsigned varenv_index;      // Index of local environment record.
        uint8_t varenv_slot;        // Count of allocated varenv slots.
        bool after_continue;        // Are we currently in code that can be skipped by continue?
        bool repeat_until;          // Are we currently in the until part of a loop?

        bool is_function() const;   // Is this the scope of a function?
        bool is_loop() const;       // Is this the scope of a loop?
        bool is_repeat() const;     // Is this the scope of a repeat/until loop?

        // Map of names to variables.
        std::unordered_map< std::string_view, variable > variables;
    };

    void visit( ast_function* f, ast_node_index node );

    void open_scope( ast_function* f, ast_node_index block, ast_node_index node );
    void declare_implicit_self( ast_function* f );
    void declare( ast_function* f, ast_node_index name_list );
    void lookup( ast_function* f, ast_node_index name, lookup_context context );
    void close_scope();

    scope* loop_scope();

    source* _source;
    ast_script* _ast_script;
    std::vector< std::unique_ptr< scope > > _scopes;
};

}

#endif

