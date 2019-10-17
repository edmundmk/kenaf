//
//  ast_resolve.cpp
//
//  Created by Edmund Kapusniak on 30/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ast_resolve.h"

namespace kf
{

inline bool ast_resolve::scope::is_function() const
{
    return function->nodes.at( node_index ).kind == AST_FUNCTION;
}

inline bool ast_resolve::scope::is_loop() const
{
    ast_node_kind kind = function->nodes.at( node_index ).kind;
    return kind == AST_STMT_FOR_STEP || kind == AST_STMT_FOR_EACH
        || kind == AST_STMT_WHILE || kind == AST_STMT_REPEAT;
}

inline bool ast_resolve::scope::is_repeat() const
{
    ast_node_kind kind = function->nodes.at( node_index ).kind;
    return kind == AST_STMT_REPEAT;
}

ast_resolve::ast_resolve( source* source, ast_script* ast_script )
    :   _source( source )
    ,   _ast_script( ast_script )
{
}

ast_resolve::~ast_resolve()
{
}

void ast_resolve::resolve()
{
    ast_function* function = _ast_script->functions.at( 0 ).get();
    visit( function, function->nodes.size() - 1 );
    assert( _scopes.empty() );
}

void ast_resolve::visit( ast_function* f, unsigned index )
{
    ast_node* n = &f->nodes[ index ];
    unsigned until_index = AST_INVALID_INDEX;

    switch ( n->kind )
    {
    case AST_EXPR_UNPACK:
    {
        // Look up name inside unpack, allow vararg parameters.
        unsigned value_index = n->child_index;
        if ( f->nodes[ value_index ].kind == AST_NAME )
        {
            lookup( f, value_index, LOOKUP_UNPACK );
            return;
        }
    }

    case AST_DECL_VAR:
    {
        // Variable declarations.
        unsigned name_list_index = n->child_index;
        unsigned rval_list_index = f->nodes[ name_list_index ].next_index;
        if ( rval_list_index < index )
        {
            visit( f, rval_list_index );
        }
        declare( f, name_list_index );
        return;
    }

    case AST_DECL_DEF:
    {
        // Declare a def of an object.
        unsigned name_index = n->child_index;
        unsigned def_index = f->nodes[ name_index ].next_index;
        if ( f->nodes[ name_index ].kind == AST_NAME )
        {
            declare( f, name_index );
            visit( f, def_index );
            return;
        }

        // Not a single name, so the name has to resolve.
        break;
    }

    case AST_RVAL_ASSIGN:
    case AST_RVAL_OP_ASSIGN:
    {
        // Visit lvals.
        unsigned lval_index = n->child_index;
        ast_node* lval_list = &f->nodes[ lval_index ];

        // Might be a single value or a list.
        unsigned head_index;
        unsigned last_index;
        if ( lval_list->kind != AST_LVAL_LIST )
        {
            head_index = lval_index;
            last_index = lval_list->next_index;
        }
        else
        {
            head_index = lval_list->child_index;
            last_index = lval_index;
        }

        // Visit all expressions on lhs, disallowing bare global names.
        unsigned next_index;
        for ( unsigned c = head_index; c < last_index; c = next_index )
        {
            ast_node* lval = &f->nodes[ c ];
            if ( lval->kind == AST_NAME )
                lookup( f, c, LOOKUP_ASSIGN );
            else
                visit( f, c );
            next_index = lval->next_index;
        }

        // Visit remaining parts of expression.
        for ( unsigned c = lval_list->next_index; c < index; c = f->nodes[ c ].next_index )
        {
            visit( f, c );
        }

        return;
    }

    case AST_BLOCK:
    {
        // Open scope at start of any other block.
        open_scope( f, index, index );
        break;
    }

    case AST_STMT_FOR_STEP:
    {
        // For loops should always be contained in a block, giving the
        // iteration variable a scope which spans the entire loop.
        unsigned name_index = n->child_index;
        unsigned start_index = f->nodes[ name_index ].next_index;
        unsigned stop_index = f->nodes[ start_index ].next_index;
        unsigned step_index = f->nodes[ stop_index ].next_index;
        unsigned block_index = f->nodes[ step_index ].next_index;

        // Create hidden for step variable.
        ast_local local = {};
        local.name = "$for_step";
        assert( n->leaf == AST_LEAF_INDEX );
        n->leaf_index().index = f->locals.size();
        f->locals.push_back( local );

        // Declare names and visit expressions.
        visit( f, start_index );
        visit( f, stop_index );
        visit( f, step_index );
        declare( f, name_index );

        // Open loop and continue with contents of block.
        open_scope( f, block_index, index );
        index = block_index;
        n = &f->nodes[ index ];
        assert( n->kind == AST_BLOCK );
        break;
    }

    case AST_STMT_FOR_EACH:
    {
        // For loops should always be contained in a block, giving the
        // iteration variable a scope which spans the entire loop.
        unsigned name_list_index = n->child_index;
        unsigned expr_index = f->nodes[ name_list_index ].next_index;
        unsigned block_index = f->nodes[ expr_index ].next_index;

        // Create hidden for each variable.
        ast_local local = {};
        local.name = "$for_each";
        assert( n->leaf == AST_LEAF_INDEX );
        n->leaf_index().index = f->locals.size();
        f->locals.push_back( local );

        // Declare names and visit expression.
        visit( f, expr_index );
        declare( f, name_list_index );

        // Open loop and continue with contents of block.
        open_scope( f, block_index, index );
        n = &f->nodes[ index = block_index ];
        assert( n->kind == AST_BLOCK );
        break;
    }

    case AST_STMT_WHILE:
    {
        // Loop scope.
        unsigned expr_index = n->child_index;
        unsigned block_index = f->nodes[ expr_index ].next_index;

        // Test expression.
        visit( f, expr_index );

        // Open loop and continue with contents of block.
        open_scope( f, block_index, index );
        n = &f->nodes[ index = block_index ];
        assert( n->kind == AST_BLOCK );
        break;
    }

    case AST_STMT_REPEAT:
    {
        // Loop scope.  Remember 'until' as it has special scoping rules.
        unsigned block_index = n->child_index;
        until_index = f->nodes[ block_index ].next_index;

        // Open loop and continue with contents of block.
        open_scope( f, block_index, index );
        n = &f->nodes[ index = block_index ];
        assert( n->kind == AST_BLOCK );
        break;
    }

    case AST_STMT_BREAK:
    {
        // Handle break.
        scope* loop = loop_scope();
        if ( ! loop )
        {
            _source->error( n->sloc, "invalid 'break' outside of loop" );
        }
        return;
    };

    case AST_STMT_CONTINUE:
    {
        // Handle continue.
        scope* loop = loop_scope();
        if ( loop )
        {
            // Locals declared after first continue need to be marked.
            if ( loop->is_repeat() )
            {
                loop->after_continue = true;
            }
        }
        else
        {
            _source->error( n->sloc, "invalid 'continue' outside of loop" );
        }
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
        n = &f->nodes[ index = block_index ];
        assert( n->kind == AST_BLOCK );
        break;
    }

    case AST_DEF_FUNCTION:
    {
        // Visit leaf function.
        ast_function* function = n->leaf_function().function;
        visit( function, function->nodes.size() - 1 );
        return;
    }

    case AST_DEF_OBJECT:
    {
        for ( unsigned c = n->child_index; c < index; c = f->nodes[ c ].next_index )
        {
            ast_node* child = &f->nodes[ c ];
            if ( child->kind == AST_OBJECT_PROTOTYPE )
            {
                visit( f, c );
            }
            else if ( child->kind == AST_DECL_DEF || child->kind == AST_OBJECT_KEY )
            {
                assert( child->child_index < index );
                ast_node* name = &f->nodes[ child->child_index ];
                assert( name->kind == AST_NAME );
                assert( name->next_index < index );
                name->kind = AST_OBJKEY_DECL;
                visit( f, name->next_index );
            }
            else
            {
                assert( ! "malformed AST" );
            }
        }
        return;
    }

    case AST_NAME:
    {
        // Look up unqualified name.  Disallow vararg parameters.
        lookup( f, index, LOOKUP_NORMAL );
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

void ast_resolve::open_scope( ast_function* f, unsigned block_index, unsigned node_index )
{
    std::unique_ptr< scope > s = std::make_unique< scope >();
    s->function = f;
    s->block_index = block_index;
    s->node_index = node_index;
    s->varenv_index = AST_INVALID_INDEX;
    s->varenv_slot = -1;
    s->after_continue = false;
    s->repeat_until = false;

    _scopes.push_back( std::move( s ) );
}

void ast_resolve::declare_implicit_self( ast_function* f )
{
    scope* scope = _scopes.back().get();

    ast_local local = {};
    local.name = "self";
    local.is_self = true;
    local.is_parameter = true;

    unsigned local_index = f->locals.size();
    scope->variables.emplace( local.name, variable{ local_index, scope->after_continue } );
    scope->variables.emplace( "super", variable{ local_index, scope->after_continue, true } );
    f->locals.push_back( local );

    f->parameter_count += 1;
}

void ast_resolve::declare( ast_function* f, unsigned index )
{
    scope* scope = _scopes.back().get();
    ast_node* n = &f->nodes[ index ];

    assert( n->kind == AST_NAME || n->kind == AST_NAME_LIST || n->kind == AST_PARAMETERS );
    bool is_parameter = n->kind == AST_PARAMETERS;

    // Might be a name list.
    unsigned name_index;
    unsigned last_index;
    if ( n->kind == AST_NAME )
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
        bool is_vararg = false;
        if ( n->kind == AST_VARARG_PARAM )
        {
            assert( is_parameter );
            n = &f->nodes[ n->child_index ];
            is_vararg = true;
            f->is_varargs = true;
        }

        // Find name.
        assert( n->kind == AST_NAME );
        std::string_view name( n->leaf_string().text, n->leaf_string().size );

        // Check if this scope already has a local with this name.
        auto i = scope->variables.find( name );
        if ( i != scope->variables.end() )
        {
            if ( i->second.is_outenv )
                _source->error( n->sloc, "redeclaration of captured variable '%.*s'", (int)name.size(), name.data() );
            else
                _source->error( n->sloc, "redeclaration of '%.*s'", (int)name.size(), name.data() );
            continue;
        }

        // Add local.
        ast_local local = {};
        local.name = name;
        local.is_parameter = is_parameter;
        local.is_vararg = is_vararg;

        unsigned local_index = f->locals.size();
        scope->variables.emplace( local.name, variable{ local_index, scope->after_continue } );
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

void ast_resolve::lookup( ast_function* f, unsigned index, lookup_context context )
{
    ast_node* n = &f->nodes[ index ];
    scope* current_scope = _scopes.back().get();

    assert( n->kind == AST_NAME );
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
        else if ( scope_index == 0 )
        {
            // Name was not found at all.
            n->kind = AST_GLOBAL_NAME;

            // Can't assign to a bare global.
            if ( context == LOOKUP_ASSIGN )
            {
                _source->error( n->sloc, "cannot assign to undeclared identifier '%.*s'", (int)name.size(), name.data() );
            }

            return;
        }
    }

    assert( v );

    // Check for continue/until scope restriction.
    if ( current_scope->repeat_until && v->after_continue )
    {
        _source->error( n->sloc, "variable '%.*s', declared after continue, cannot be used in until expression", (int)name.size(), name.data() );
    }

    // Can't assign to super.
    if ( context == LOOKUP_ASSIGN && v->implicit_super )
    {
        _source->error( n->sloc, "cannot assign to 'super'" );
    }

    // Found in scope at scope_index.
    size_t vscope_index = scope_index++;
    scope* vscope = _scopes.at( vscope_index ).get();

    // Can't use a varargs param in anything other than an unpack expression,
    // and we can't capture a varargs param in a function closure.
    const ast_local& local = vscope->function->locals.at( v->index );
    if ( local.is_vararg )
    {
        if ( context != LOOKUP_UNPACK )
        {
            _source->error( n->sloc, "variable argument parameter '%.*s' cannot be used in an expression", (int)name.size(), name.data() );
        }

        if ( vscope->function != current_scope->function )
        {
            _source->error( n->sloc, "variable argument parameter '%.*s' cannot be captured by a closure", (int)name.size(), name.data() );
        }
    }

    if ( v->implicit_super && vscope->function != current_scope->function )
    {
        _source->error( n->sloc, "'super' cannot be captured by a closure" );
    }

    // Capture into inner function.
    while ( vscope->function != current_scope->function )
    {
        // Find next inner function scope.
        scope* outer = vscope;
        scope* inner = vscope;
        while ( inner->function == outer->function )
        {
            inner = _scopes.at( scope_index++ ).get();
        }
        assert( inner->is_function() );

        unsigned outenv_index = -1;
        uint8_t outenv_slot = -1;

        if ( v->is_outenv )
        {
            // Variable is already in an outenv.  Search for the outenv in
            // inner function's list of outenvs.
            for ( outenv_index = 0; outenv_index < inner->function->outenvs.size(); ++outenv_index )
            {
                const ast_outenv& outenv = inner->function->outenvs[ outenv_index ];
                if ( outenv.outer_outenv && outenv.outer_index == v->index )
                {
                    break;
                }
            }

            if ( outenv_index >= inner->function->outenvs.size() )
            {
                inner->function->outenvs.push_back( { v->index, true } );
            }

            outenv_slot = v->outenv_slot;
        }
        else
        {
            // Variable is a local captured from the outer function.
            assert( outer->function == vscope->function );
            ast_local* local = &vscope->function->locals.at( v->index );

            // Allocate slot in varenv of this local's block.
            if ( local->varenv_index == AST_INVALID_INDEX )
            {
                // Create block's environment.
                if ( vscope->varenv_index == AST_INVALID_INDEX )
                {
                    vscope->varenv_index = vscope->function->locals.size();
                    vscope->varenv_slot = 0;
                    ast_local varenv;
                    varenv.name = "$varenv";
                    vscope->function->locals.push_back( varenv );
                    local = &vscope->function->locals.at( v->index );
                }

                assert( vscope->varenv_index != AST_INVALID_INDEX );
                local->varenv_index = vscope->varenv_index;
                local->varenv_slot = vscope->varenv_slot++;
            }

            // Search for resulting outenv in inner function's list of outenvs.
            for ( outenv_index = 0; outenv_index < inner->function->outenvs.size(); ++outenv_index )
            {
                const ast_outenv& outenv = inner->function->outenvs[ outenv_index ];
                if ( ! outenv.outer_outenv && outenv.outer_index == local->varenv_index )
                {
                    break;
                }
            }

            if ( outenv_index >= inner->function->outenvs.size() )
            {
                inner->function->outenvs.push_back( { local->varenv_index, false } );
            }

            outenv_slot = local->varenv_slot;
        }

        // Add entry to inner function's scope to accelerate subsequent
        // searches for this same upval, and to disallow redeclaration of
        // captured variables at function scope.
        auto inserted = inner->variables.insert_or_assign( name, variable{ outenv_index, false, false, true, outenv_slot } );
        assert( inserted.second );

        // Variable capture continues with this new variable.
        v = &inserted.first->second;
        vscope = inner;
    }

    // Make reference to variable.
    assert( vscope->function == current_scope->function );
    assert( n->leaf );
    if ( ! v->is_outenv )
    {
        n->kind = v->implicit_super ? AST_SUPER_NAME : AST_LOCAL_NAME;
        n->leaf = AST_LEAF_INDEX;
        n->leaf_index().index = v->index;
    }
    else
    {
        n->kind = AST_OUTENV_NAME;
        n->leaf = AST_LEAF_OUTENV;
        n->leaf_outenv() = { v->index, v->outenv_slot };
    }
}

void ast_resolve::close_scope()
{
    // Pop scope.
    std::unique_ptr< scope > s = std::move( _scopes.back() );
    _scopes.pop_back();

    // Set varenv.
    if ( s->varenv_index != AST_INVALID_INDEX )
    {
        ast_local* varenv = &s->function->locals.at( s->varenv_index );
        varenv->varenv_slot = s->varenv_slot;

        ast_node* node = &s->function->nodes.at( s->block_index );
        node->leaf_index().index = s->varenv_index;
    }
}

ast_resolve::scope* ast_resolve::loop_scope()
{
    for ( auto i = _scopes.rbegin(); i != _scopes.rend(); ++i )
    {
        scope* s = i->get();
        if ( s->is_loop() )
        {
            return s;
        }
    }

    return nullptr;
}


}

