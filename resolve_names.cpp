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
    syntax_function* function = _syntax_tree->functions.at( 0 ).get();
    visit( function, function->nodes.size() - 1 );
}

void resolve_names::visit( syntax_function* f, unsigned index )
{
    syntax_node* n = &f->nodes[ index ];
    unsigned until_index = AST_INVALID_INDEX;

    switch ( n->kind )
    {
    case AST_DEF_FUNCTION:
    {
        // Visit leaf function.
        syntax_function* function = n->leaf_function().function;
        visit( function, function->nodes.size() - 1 );
        return;
    }

    case AST_FUNCTION:
    {
        // Functions declare parameters into the block scope.
        unsigned parameters_index = n->child_index;
        unsigned block_index = f->nodes[ parameters_index ].next_index;

        // Open scope and declare parameters.
        open_scope( f, block_index, index );
        if ( f->implicit_self )
        {
            declare_implicit_self( f );
        }
        declare( f, parameters_index );

        // Continue with block.
        index = block_index;
        n = &f->nodes[ index ];
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
        index = block_index;
        n = &f->nodes[ index ];
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
        index = block_index;
        n = &f->nodes[ index ];
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
        index = block_index;
        n = &f->nodes[ index ];
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
        index = block_index;
        n = &f->nodes[ index ];
        break;
    }

    case AST_STMT_CONTINUE:
    {
        // Handle continue.
        for ( auto i = _scopes.rbegin(); i != _scopes.rend(); ++i )
        {
            syntax_node* n = &f->nodes[ ( *i )->node_index ];
            if ( n->kind == AST_STMT_FOR_STEP || n->kind == AST_STMT_FOR_EACH
                || n->kind == AST_STMT_WHILE || n->kind == AST_STMT_REPEAT )
            {
                ( *i )->after_continue = true;
                break;
            }
        }
        return;
    }

    case AST_BLOCK:
    {
        // Open scope at start of any other block.
        open_scope( f, index, index );
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
        _scopes.back()->repeat_until = true;
        visit( f, until_index );
    }

    // Close scope at end of block.
    if ( n->kind == AST_BLOCK )
    {
        close_scope();
    }
}

void resolve_names::open_scope( syntax_function* f, unsigned block_index, unsigned node_index )
{
    std::unique_ptr< scope > s = std::make_unique< scope >();
    s->function = f;
    s->block_index = block_index;
    s->node_index = node_index;
    s->after_continue = false;
    s->repeat_until = _scopes.size() ? _scopes.back()->repeat_until : false;
    _scopes.push_back( std::move( s ) );
}

void resolve_names::declare_implicit_self( syntax_function* f )
{
    scope* scope = _scopes.back().get();

    syntax_local local = {};
    local.name = "self";
    local.downval_index = AST_INVALID_INDEX;
    local.is_implicit_self = true;
    local.is_parameter = true;

    unsigned local_index = f->locals.size();
    scope->variables.emplace( local.name, variable{ local_index, false, scope->after_continue } );
    scope->variables.emplace( "super", variable{ local_index, true, scope->after_continue } );
    f->locals.push_back( local );

    f->parameter_count += 1;
}

void resolve_names::declare( syntax_function* f, unsigned index )
{
    scope* scope = _scopes.back().get();
    syntax_node* n = &f->nodes[ index ];

    assert( n->kind == AST_EXPR_NAME || n->kind == AST_NAME_LIST || n->kind == AST_PARAMETERS );
    bool is_parameter = n->kind == AST_PARAMETERS;

    // Might be a name list.
    unsigned name_index;
    unsigned last_index;
    if ( n->kind == AST_EXPR_NAME )
    {
        name_index = index;
        last_index = n->next_index;
    }
    else
    {
        name_index = n->child_index;
        last_index = index;
    }

    // Declare all names in list.
    unsigned next_index;
    for ( ; name_index < last_index; name_index = next_index )
    {
        n = &f->nodes[ name_index ];
        next_index = n->next_index;

        // Check for varargs param.
        bool is_vararg_param = false;
        if ( n->kind == AST_VARARG_PARAM )
        {
            assert( is_parameter );
            n = &f->nodes[ n->child_index ];
            is_vararg_param = true;
            f->is_varargs = true;
        }

        // Find name.
        assert( n->kind == AST_EXPR_NAME );
        std::string_view name( n->leaf_string().text, n->leaf_string().size );

        // Check if this scope already has a local with this name.
        auto i = scope->variables.find( name );
        if ( i != scope->variables.end() )
        {
            _source->error( n->sloc, "redeclaration of '%.*s'", (int)name.size(), name.data() );
            continue;
        }

        // Add local.
        syntax_local local = {};
        local.name = name;
        local.downval_index = AST_INVALID_INDEX;
        local.is_parameter = is_parameter;
        local.is_vararg_param = is_vararg_param;

        unsigned local_index = f->locals.size();
        scope->variables.emplace( local.name, variable{ local_index, false, scope->after_continue } );
        f->locals.push_back( local );

        if ( is_parameter )
        {
            f->parameter_count += 1;
        }

        // Replace EXPR_NAME with LOCAL_DECL.
        assert( n->leaf );
        n->kind = AST_LOCAL_DECL;
        n->leaf = AST_LEAF_INDEX;
        n->leaf_index().index = local_index;
    }
}

void resolve_names::lookup( syntax_function* f, unsigned index )
{
    syntax_node* n = &f->nodes[ index ];
    scope* current_scope = _scopes.back().get();

    assert( n->kind == AST_EXPR_NAME );
    std::string_view name( n->leaf_string().text, n->leaf_string().size );

    // Search for name in each scope in turn.
    variable* v = nullptr;
    size_t scope_index = _scopes.size();
    while ( scope_index-- )
    {
        // Search for it.
        scope* s = _scopes.at( scope_index ).get();
        auto i = s->variables.find( name );
        if ( i != s->variables.end() )
        {
            // Found, complete the search.
            v = &i->second;
            break;
        }

        if ( scope_index != 0 )
        {
            // Not found.
            continue;
        }
        else
        {
            // Not found at all.
            n->kind = AST_GLOBAL_NAME;
            return;
        }
    }

    assert( v );

    // Check for continue/until scope restriction.
    if ( current_scope->repeat_until && v->after_continue )
    {
        _source->error( n->sloc, "variable '%.*s', declared after continue, cannot be used in until expression", (int)name.size(), name.data() );
    }

    // Found in scope at scope_index.
    scope* s = _scopes.at( scope_index ).get();
    if ( s->function == current_scope->function )
    {
        // Not an upval.
        assert( n->leaf );
        n->kind = v->implicit_super ? AST_LOCAL_NAME_SUPER : AST_LOCAL_NAME;
        n->leaf = AST_LEAF_INDEX;
        n->leaf_index().index = v->local_index;
    }

    // It's an upval, need to ensure downvals.
}

void resolve_names::close_scope()
{
    _scopes.pop_back();
}

}

