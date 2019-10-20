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
    return function->nodes[ node_index ].kind == AST_FUNCTION;
}

inline bool ast_resolve::scope::is_loop() const
{
    ast_node_kind kind = function->nodes[ node_index ].kind;
    return kind == AST_STMT_FOR_STEP
        || kind == AST_STMT_FOR_EACH
        || kind == AST_STMT_WHILE
        || kind == AST_STMT_REPEAT;
}

inline bool ast_resolve::scope::is_repeat() const
{
    ast_node_kind kind = function->nodes[ node_index ].kind;
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
    ast_function* function = _ast_script->functions[ 0 ].get();
    unsigned last_index = function->nodes.size() - 1;
    visit( function, { &function->nodes[ last_index ], last_index } );
    assert( _scopes.empty() );
}

void ast_resolve::visit( ast_function* f, ast_node_index node )
{
    ast_node_index until = { nullptr, AST_INVALID_INDEX };

    switch ( node->kind )
    {
    case AST_EXPR_UNPACK:
    {
        // Look up name inside unpack, allow vararg parameters.
        ast_node_index value = ast_child_node( f, node );
        if ( value->kind == AST_NAME )
        {
            lookup( f, value, LOOKUP_UNPACK );
            return;
        }
        break;
    }

    case AST_DECL_VAR:
    {
        // Variable declarations.
        ast_node_index name_list = ast_child_node( f, node );
        ast_node_index rval_list = ast_next_node( f, name_list );
        if ( rval_list.index < node.index )
        {
            visit( f, rval_list );
        }
        declare( f, name_list );
        return;
    }

    case AST_DECL_DEF:
    {
        // Declare a def of an object.
        ast_node_index name = ast_child_node( f, node );
        ast_node_index def = ast_next_node( f, name );
        if ( name->kind == AST_NAME )
        {
            declare( f, name );
            visit( f, def );
            return;
        }

        // Not a single name, so the name has to resolve.
        break;
    }

    case AST_RVAL_ASSIGN:
    case AST_RVAL_OP_ASSIGN:
    {
        // Visit lvals.
        ast_node_index lval_list = ast_child_node( f, node );

        // Might be a single value or a list.
        ast_node_index head;
        ast_node_index last;
        if ( lval_list->kind != AST_LVAL_LIST )
        {
            head = lval_list;
            last = ast_next_node( f, lval_list );
        }
        else
        {
            head = ast_child_node( f, lval_list );
            last = lval_list;
        }

        // Visit all expressions on lhs, disallowing bare global names.
        for ( ast_node_index lval = head; lval.index < last.index; lval = ast_next_node( f, lval ) )
        {
            if ( lval->kind == AST_NAME )
                lookup( f, lval, LOOKUP_ASSIGN );
            else
                visit( f, lval );
        }

        // Visit remaining parts of expression.
        for ( ast_node_index child = ast_next_node( f, lval_list ); child.index < node.index; child = ast_next_node( f, child ) )
        {
            visit( f, child );
        }

        return;
    }

    case AST_BLOCK:
    {
        // Open scope at start of any other block.
        open_scope( f, node, node );
        break;
    }

    case AST_STMT_FOR_STEP:
    {
        // For loops should always be contained in a block, giving the
        // iteration variable a scope which spans the entire loop.
        ast_node_index name = ast_child_node( f, node );
        ast_node_index start = ast_next_node( f, name );
        ast_node_index stop = ast_next_node( f, start );
        ast_node_index step = ast_next_node( f, stop );
        ast_node_index block = ast_next_node( f, step );

        // Create hidden for step variable.
        ast_local local = {};
        local.name = "$for_step";
        assert( node->leaf == AST_LEAF_INDEX );
        node->leaf_index().index = f->locals.append( local );

        // Declare names and visit expressions.
        visit( f, start );
        visit( f, stop );
        visit( f, step );
        declare( f, name );

        // Open loop and continue with contents of block.
        open_scope( f, block, node );
        node = block;
        assert( node->kind == AST_BLOCK );
        break;
    }

    case AST_STMT_FOR_EACH:
    {
        // For loops should always be contained in a block, giving the
        // iteration variable a scope which spans the entire loop.
        ast_node_index name_list = ast_child_node( f, node );
        ast_node_index expr = ast_next_node( f, name_list );
        ast_node_index block = ast_next_node( f, expr );

        // Create hidden for each variable.
        ast_local local = {};
        local.name = "$for_each";
        assert( node->leaf == AST_LEAF_INDEX );
        node->leaf_index().index = f->locals.append( local );

        // Declare names and visit expression.
        visit( f, expr );
        declare( f, name_list );

        // Open loop and continue with contents of block.
        open_scope( f, block, node );
        node = block;
        assert( node->kind == AST_BLOCK );
        break;
    }

    case AST_STMT_WHILE:
    {
        // Loop scope.
        ast_node_index expr = ast_child_node( f, node );
        ast_node_index block = ast_next_node( f, expr );

        // Test expression.
        visit( f, expr );

        // Open loop and continue with contents of block.
        open_scope( f, block, node );
        node = block;
        assert( node->kind == AST_BLOCK );
        break;
    }

    case AST_STMT_REPEAT:
    {
        // Loop scope.  Remember 'until' as it has special scoping rules.
        ast_node_index block = ast_child_node( f, node );
        until = ast_next_node( f, block );

        // Open loop and continue with contents of block.
        open_scope( f, block, node );
        node = block;
        assert( node->kind == AST_BLOCK );
        break;
    }

    case AST_STMT_BREAK:
    {
        // Handle break.
        scope* loop = loop_scope();
        if ( ! loop )
        {
            _source->error( node->sloc, "invalid 'break' outside of loop" );
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
            _source->error( node->sloc, "invalid 'continue' outside of loop" );
        }
        return;
    }

    case AST_FUNCTION:
    {
        // Functions declare parameters into the block scope.
        ast_node_index parameters = ast_child_node( f, node );
        ast_node_index block = ast_next_node( f, parameters );

        // Open scope and declare parameters.
        open_scope( f, block, node );
        if ( f->implicit_self )
        {
            declare_implicit_self( f );
        }
        declare( f, parameters );

        // Continue with block.
        node = block;
        assert( node->kind == AST_BLOCK );
        break;
    }

    case AST_DEF_FUNCTION:
    {
        // Visit leaf function.
        ast_function* function = node->leaf_function().function;
        unsigned last_index = function->nodes.size() - 1;
        visit( function, { &function->nodes[ last_index ], last_index } );
        return;
    }

    case AST_DEF_OBJECT:
    {
        for ( ast_node_index child = ast_child_node( f, node ); child.index < node.index; child = ast_next_node( f, child ) )
        {
            if ( child->kind == AST_OBJECT_PROTOTYPE )
            {
                visit( f, child );
            }
            else if ( child->kind == AST_DECL_DEF || child->kind == AST_OBJECT_KEY )
            {
                ast_node_index name = ast_child_node( f, child );
                assert( name.index < node.index );
                assert( name->kind == AST_NAME );
                ast_node_index decl = ast_next_node( f, name );
                assert( decl.index < node.index );
                name->kind = AST_OBJKEY_DECL;
                visit( f, decl );
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
        lookup( f, node, LOOKUP_NORMAL );
        return;
    }

    default: break;
    }

    // Visit children.
    for ( ast_node_index child = ast_child_node( f, node ); child.index < node.index; child = ast_next_node( f, child ) )
    {
        visit( f, child );
    }

    // Deal with 'until' expression, which cannot use names after continue.
    if ( until.index != AST_INVALID_INDEX )
    {
        _scopes.back()->repeat_until = true;
        visit( f, until );
    }

    // Close scope at end of block.
    if ( node->kind == AST_BLOCK )
    {
        close_scope();
    }
}

void ast_resolve::open_scope( ast_function* f, ast_node_index block, ast_node_index node )
{
    std::unique_ptr< scope > s = std::make_unique< scope >();
    s->function = f;
    s->block_index = block.index;
    s->node_index = node.index;
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

    unsigned local_index = f->locals.append( local );
    scope->variables.emplace( local.name, variable{ local_index, scope->after_continue } );
    scope->variables.emplace( "super", variable{ local_index, scope->after_continue, true } );

    f->parameter_count += 1;
}

void ast_resolve::declare( ast_function* f, ast_node_index name_list )
{
    scope* scope = _scopes.back().get();

    assert( name_list->kind == AST_NAME || name_list->kind == AST_NAME_LIST || name_list->kind == AST_PARAMETERS );
    bool is_parameter = name_list->kind == AST_PARAMETERS;

    // Might be a name list.
    ast_node_index name;
    ast_node_index last;
    if ( name_list->kind == AST_NAME )
    {
        name = name_list;
        last = ast_next_node( f, name_list );
    }
    else
    {
        name = ast_child_node( f, name_list );
        last = name_list;
    }

    // Declare all names in list.
    ast_node_index next;
    for ( ; name.index < last.index; name = next )
    {
        next = ast_next_node( f, name );

        // Check for varargs param.
        bool is_vararg = false;
        if ( name->kind == AST_VARARG_PARAM )
        {
            assert( is_parameter );
            name = ast_child_node( f, name );
            is_vararg = true;
            f->is_varargs = true;
        }

        // Find name.
        assert( name->kind == AST_NAME );
        std::string_view text( name->leaf_string().text, name->leaf_string().size );

        // Check if this scope already has a local with this name.
        auto i = scope->variables.find( text );
        if ( i != scope->variables.end() )
        {
            if ( i->second.is_outenv )
                _source->error( name->sloc, "redeclaration of captured variable '%.*s'", (int)text.size(), text.data() );
            else
                _source->error( name->sloc, "redeclaration of '%.*s'", (int)text.size(), text.data() );
            continue;
        }

        // Add local.
        ast_local local = {};
        local.name = text;
        local.is_parameter = is_parameter;
        local.is_vararg = is_vararg;

        unsigned local_index = f->locals.append( local );
        scope->variables.emplace( local.name, variable{ local_index, scope->after_continue } );

        if ( is_parameter )
        {
            f->parameter_count += 1;
        }

        // Replace EXPR_NAME with LOCAL_DECL.
        assert( name->leaf );
        name->kind = AST_LOCAL_DECL;
        name->leaf = AST_LEAF_INDEX;
        name->leaf_index().index = local_index;
    }
}

void ast_resolve::lookup( ast_function* f, ast_node_index name, lookup_context context )
{
    scope* current_scope = _scopes.back().get();

    assert( name->kind == AST_NAME );
    std::string_view text( name->leaf_string().text, name->leaf_string().size );

    // Search for name in each scope in turn.
    variable* v = nullptr;
    size_t scope_index = _scopes.size();
    while ( scope_index-- )
    {
        // Search for it.
        scope* s = _scopes.at( scope_index ).get();
        auto i = s->variables.find( text );
        if ( i != s->variables.end() )
        {
            // Found, complete the search.
            v = &i->second;
            break;
        }
        else if ( scope_index == 0 )
        {
            // Name was not found at all.
            name->kind = AST_GLOBAL_NAME;

            // Can't assign to a bare global.
            if ( context == LOOKUP_ASSIGN )
            {
                _source->error( name->sloc, "cannot assign to undeclared identifier '%.*s'", (int)text.size(), text.data() );
            }

            return;
        }
    }

    assert( v );

    // Check for continue/until scope restriction.
    if ( current_scope->repeat_until && v->after_continue )
    {
        _source->error( name->sloc, "variable '%.*s', declared after continue, cannot be used in until expression", (int)text.size(), text.data() );
    }

    // Can't assign to super.
    if ( context == LOOKUP_ASSIGN && v->implicit_super )
    {
        _source->error( name->sloc, "cannot assign to 'super'" );
    }

    // Found in scope at scope_index.
    size_t vscope_index = scope_index++;
    scope* vscope = _scopes.at( vscope_index ).get();

    // Can't use a varargs param in anything other than an unpack expression,
    // and we can't capture a varargs param in a function closure.
    const ast_local& local = vscope->function->locals[ v->index ];
    if ( local.is_vararg )
    {
        if ( context != LOOKUP_UNPACK )
        {
            _source->error( name->sloc, "variable argument parameter '%.*s' cannot be used in an expression", (int)text.size(), text.data() );
        }

        if ( vscope->function != current_scope->function )
        {
            _source->error( name->sloc, "variable argument parameter '%.*s' cannot be captured by a closure", (int)text.size(), text.data() );
        }
    }

    if ( v->implicit_super && vscope->function != current_scope->function )
    {
        _source->error( name->sloc, "'super' cannot be captured by a closure" );
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
                outenv_index = inner->function->outenvs.append( { v->index, true } );
            }

            outenv_slot = v->outenv_slot;
        }
        else
        {
            // Variable is a local captured from the outer function.
            assert( outer->function == vscope->function );
            ast_local* local = &vscope->function->locals[ v->index ];

            // Allocate slot in varenv of this local's block.
            if ( local->varenv_index == AST_INVALID_INDEX )
            {
                // Create block's environment.
                if ( vscope->varenv_index == AST_INVALID_INDEX )
                {
                    ast_local varenv;
                    varenv.name = "$varenv";
                    vscope->varenv_index = vscope->function->locals.append( varenv );
                    vscope->varenv_slot = 0;
                    local = &vscope->function->locals[ v->index ];
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
                outenv_index = inner->function->outenvs.append( { local->varenv_index, false } );
            }

            outenv_slot = local->varenv_slot;
        }

        // Add entry to inner function's scope to accelerate subsequent
        // searches for this same upval, and to disallow redeclaration of
        // captured variables at function scope.
        auto inserted = inner->variables.insert_or_assign( text, variable{ outenv_index, false, false, true, outenv_slot } );
        assert( inserted.second );

        // Variable capture continues with this new variable.
        v = &inserted.first->second;
        vscope = inner;
    }

    // Make reference to variable.
    assert( vscope->function == current_scope->function );
    assert( name->leaf );
    if ( ! v->is_outenv )
    {
        name->kind = v->implicit_super ? AST_SUPER_NAME : AST_LOCAL_NAME;
        name->leaf = AST_LEAF_INDEX;
        name->leaf_index().index = v->index;
    }
    else
    {
        name->kind = AST_OUTENV_NAME;
        name->leaf = AST_LEAF_OUTENV;
        name->leaf_outenv() = { v->index, v->outenv_slot };
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
        ast_local* varenv = &s->function->locals[ s->varenv_index ];
        varenv->varenv_slot = s->varenv_slot;

        ast_node* node = &s->function->nodes[ s->block_index ];
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

