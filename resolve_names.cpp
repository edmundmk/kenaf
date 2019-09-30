//
//  resolve_names.cpp
//
//  Created by Edmund Kapusniak on 30/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "resolve_names.h"

namespace kf
{

resolve_names::resolve_names( source* source, syntax_tree* syntax_tree )
    :   _source( source )
    ,   _syntax_tree( syntax_tree )
{
}

resolve_names::~resolve_names()
{
}

void resolve_names::resolve()
{

}

void resolve_names::visit( syntax_function* f, unsigned index )
{
    const syntax_node* n = &f->nodes[ index ];
    unsigned until_index = AST_INVALID_INDEX;

    switch ( n->kind )
    {
    case AST_FUNCTION:
    {
        // Functions declare parameters into the block scope.
        unsigned parameters_index = n->child_index;
        unsigned block_index = f->nodes[ parameters_index ].next_index;

        // Open scope and declare parameters.
        open_scope( f, block_index, AST_INVALID_INDEX );
        declare( f, parameters_index );

        // Continue with block.
        n = &f->nodes[ block_index ];
        break;
    }

    case AST_STMT_FOR_STEP:
    {
        // For loops declare variables into the block scope.
        unsigned name_index = n->child_index;
        unsigned start_index = f->nodes[ name_index ].next_index;
        unsigned stop_index = f->nodes[ start_index ].next_index;
        unsigned step_index = f->nodes[ stop_index ].next_index;
        unsigned block_index = f->nodes[ step_index ].next_index;

        // Open scope and declare name, then visit expressions.
        open_scope( f, block_index, index );
        declare( f, name_index );
        visit( f, start_index );
        visit( f, stop_index );
        visit( f, step_index );

        // Continue with contents of block.
        n = &f->nodes[ block_index ];
        break;
    }

    case AST_STMT_FOR_EACH:
    {
        // For loops declare variables into the block scope.
        unsigned name_list_index = n->child_index;
        unsigned expr_index = f->nodes[ name_list_index ].next_index;
        unsigned block_index = f->nodes[ expr_index ].next_index;

        // Declare names and visit expression.
        open_scope( f, block_index, index );
        declare( f, name_list_index );
        visit( f, expr_index );

        // Continue with contents of block.
        n = &f->nodes[ block_index ];
        break;
    }

    case AST_STMT_WHILE:
    {
        // Loop scope.
        unsigned expr_index = n->child_index;
        unsigned block_index = f->nodes[ expr_index ].next_index;

        // Open loop and visit expression.
        open_scope( f, block_index, index );
        visit( f, expr_index );

        // Continue with contents of block.
        n = &f->nodes[ block_index ];
        break;
    }

    case AST_STMT_REPEAT:
    {
        // Loop scope.  Remember 'until' as it has special scoping rules.
        unsigned block_index = n->child_index;
        until_index = f->nodes[ block_index ].next_index;

        // Open loop.
        open_scope( f, block_index, index );

        // Continue with contents of block.
        n = &f->nodes[ block_index ];
        return;
    }

    case AST_STMT_CONTINUE:
    {
        // Handle continue.
        // TODO.
        return;
    }

    case AST_BLOCK:
    {
        // Open scope at start of any other block.
        open_scope( f, index, AST_INVALID_INDEX );
        break;
    }

    case AST_STMT_VAR:
    {
        // Variable declarations.
        unsigned name_list_index = n->child_index;
        unsigned rval_list_index = f->nodes[ name_list_index ].next_index;
        declare( f, name_list_index );
        visit( f, rval_list_index );
        return;
    }

    case AST_DEFINITION:
    {
        // Declare a def of an object.
        unsigned name_index = n->child_index;
        unsigned def_index = f->nodes[ name_index ].next_index;
        if ( f->nodes[ name_index ].kind == AST_EXPR_NAME )
        {
            declare( f, name_index );
            visit( f, def_index );
            return;
        }

        // Not a single name, so the name has to resolve.
        break;
    }

    case AST_EXPR_NAME:
    {
        // Look up unqualified name.
        lookup( f, index );
        return;
    }

    default: break;
    }

    // Visit children.
    for ( unsigned c = n->child_index; c < index; c = f->nodes[ c ].next_index )
    {
        visit( f, c );
    }

    // Deal with 'until' expression, which cannot use names after continue.
    if ( until_index != AST_INVALID_INDEX )
    {
        // TODO.
        visit( f, until_index );
    }

    // Close scope at end of block.
    if ( n->kind == AST_BLOCK )
    {
        close_scope();
    }
}

void resolve_names::open_scope( syntax_function* f, unsigned block_index, unsigned loop_index )
{
}

void resolve_names::declare( syntax_function* f, unsigned index )
{
}

void resolve_names::lookup( syntax_function* f, unsigned index )
{
    const syntax_node* n = &f->nodes[ index ];
    assert( n->kind == AST_EXPR_NAME );
}

void resolve_names::close_scope()
{
}

}

