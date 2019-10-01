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

inline bool resolve_names::scope::is_function() const
{
    return function->nodes.at( node_index ).kind == AST_FUNCTION;
}

inline bool resolve_names::scope::is_loop() const
{
    syntax_node_kind kind = function->nodes.at( node_index ).kind;
    return kind == AST_STMT_FOR_STEP || kind == AST_STMT_FOR_EACH
        || kind == AST_STMT_WHILE || kind == AST_STMT_REPEAT;
}

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
            scope* s = i->get();
            if ( s->is_loop() )
            {
                s->after_continue = true;
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
    s->repeat_until = false;

    if ( s->is_function() )
    {
        s->upstack = std::make_shared< upstack >();
        s->upstack->function = f;
    }
    else
    {
        s->upstack = _scopes.back()->upstack;
        assert( s->upstack->function == f );
    }
    s->close_index = s->upstack->upstack_slots.size();

    _scopes.push_back( std::move( s ) );
}

void resolve_names::declare_implicit_self( syntax_function* f )
{
    scope* scope = _scopes.back().get();

    syntax_local local = {};
    local.name = "self";
    local.upstack_index = AST_INVALID_INDEX;
    local.is_implicit_self = true;
    local.is_parameter = true;

    unsigned local_index = f->locals.size();
    scope->variables.emplace( local.name, variable{ local_index, false, false, scope->after_continue } );
    scope->variables.emplace( "super", variable{ local_index, false, true, scope->after_continue } );
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
            if ( i->second.is_upval )
                _source->error( n->sloc, "redeclaration of captured variable '%.*s'", (int)name.size(), name.data() );
            else
                _source->error( n->sloc, "redeclaration of '%.*s'", (int)name.size(), name.data() );
            continue;
        }

        // Add local.
        syntax_local local = {};
        local.name = name;
        local.upstack_index = AST_INVALID_INDEX;
        local.is_parameter = is_parameter;
        local.is_vararg_param = is_vararg_param;

        unsigned local_index = f->locals.size();
        scope->variables.emplace( local.name, variable{ local_index, false, false, scope->after_continue } );
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
        else if ( scope_index != 0 )
        {
            // Not found in this scope, continue search.
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
    size_t vscope_index = scope_index++;
    scope* vscope = _scopes.at( vscope_index ).get();

    // Capture upvals into inner functions.
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

        // Upval might already have been added to inner function's upval list,
        // e.g. if a function captures both 'self' and 'super'.
        unsigned upval_index = 0;
        for ( ; upval_index < inner->function->upvals.size(); ++upval_index )
        {
            const syntax_upval& upval = inner->function->upvals[ upval_index ];
            if ( upval.outer_index == v->index && upval.outer_upval == v->is_upval )
            {
                break;
            }
        }

        // If we didn't find it, we have to add it.
        if ( upval_index >= inner->function->upvals.size() )
        {
            // If the variable is a local in the outer function, we have to
            // allocate it a slot on the outer function's upstack.
            if ( ! v->is_upval )
            {
                insert_upstack( vscope->upstack.get(), vscope_index, v );
                assert( outer->function->locals.at( v->index ).upstack_index != AST_INVALID_INDEX );
            }

            // Add to inner function's upval list.
            inner->function->upvals.push_back( { v->index, v->is_upval } );
        }

        // Add entry to inner function's scope to accelerate subsequent
        // searches for this same upval, and to disallow redeclaration of
        // captured variables at function scope.
        auto inserted = inner->variables.emplace( name, variable{ upval_index, true, v->implicit_super, false } );
        assert( inserted.second );

        // Variable capture continues with this new variable.
        v = &inserted.first->second;
        vscope = inner;
    }

    // Make reference to variable.
    assert( vscope->function == current_scope->function );
    assert( n->leaf );
    if ( v->is_upval )
        n->kind = v->implicit_super ? AST_UPVAL_NAME_SUPER : AST_UPVAL_NAME;
    else
        n->kind = v->implicit_super ? AST_LOCAL_NAME_SUPER : AST_LOCAL_NAME;
    n->leaf = AST_LEAF_INDEX;
    n->leaf_index().index = v->index;
}

void resolve_names::close_scope()
{
    // Pop scope.
    std::unique_ptr< scope > s = std::move( _scopes.back() );
    _scopes.pop_back();

    // Close upvals.
    close_upstack( s->upstack.get(), s->block_index, s->close_index );
}

void resolve_names::insert_upstack( upstack* upstack, size_t scope_index, const variable* variable )
{
    assert( upstack->function == _scopes.at( scope_index)->function );
    assert( ! variable->is_upval );

    /*
        Variables must be inserted into the upstack before any variables in
        child scopes.  This is because closing a child scope must close upstack
        slots for variables declared in that scope, but leave open variables
        declared in parent scopes.  However, upstack insertion happens when a
        variable is first captured, not when it is declared.  Work out which
        index this means.
    */
    unsigned insert_index = upstack->upstack_slots.size();
    if ( scope_index + 1 < _scopes.size() )
    {
        const scope* next_scope = _scopes.at( scope_index + 1 ).get();
        if ( next_scope->function == upstack->function )
        {
            insert_index = next_scope->close_index;
        }
    }

    /*
        Assign local to upstack slot.
    */
    syntax_local& local = upstack->function->locals.at( variable->index );
    assert( local.upstack_index == AST_INVALID_INDEX );
    local.upstack_index = insert_index;

    if ( insert_index >= upstack->upstack_slots.size() )
    {
        // Pushing a new upval onto the end of the stack is straightforward.
        upstack->upstack_slots.push_back( variable->index );

        // Update maximum upstack size.
        unsigned upstack_size = upstack->upstack_slots.size();
        upstack->function->max_upstack_size = std::max( upstack->function->max_upstack_size, upstack_size );

        // Done.
        return;
    }

    /*
        Otherwise, we must move upvals higher in the stack to open a slot.
        This means updating their upval indexes, and also updating the close
        index for blocks which close the stack above the insertion.
    */
    upstack->upstack_slots.insert( upstack->upstack_slots.begin() + insert_index, variable->index );

    // Update upval indexes for subsequent locals.
    for ( unsigned i = insert_index + 1; i < upstack->upstack_slots.size(); ++i )
    {
        unsigned local_index = upstack->upstack_slots.at( i );
        syntax_local& local = upstack->function->locals.at( local_index );
        assert( local.upstack_index == i - 1 );
        local.upstack_index = i;
    }

    // Update all blocks which are anchored below the inserted index, and which
    // close to an index above it.
    for ( upstack_block& close : upstack->upstack_close )
    {
        syntax_node& node = upstack->function->nodes.at( close.block_index );
        assert( node.kind == AST_BLOCK );
        assert( node.leaf == AST_LEAF_INDEX );
        assert( node.leaf_index().index >= close.floor_index );

        if ( close.floor_index < insert_index && node.leaf_index().index > insert_index )
        {
            node.leaf_index().index += 1;
        }
    }

    // Update max upstack size.
    unsigned upstack_size = upstack->upstack_slots.size();
    upstack->function->max_upstack_size = std::max( upstack->function->max_upstack_size, upstack_size );
}

void resolve_names::close_upstack( upstack* upstack, unsigned block_index, unsigned close_index )
{
    // Get block node to close.
    syntax_node& node = upstack->function->nodes.at( block_index );
    assert( node.kind == AST_BLOCK );
    assert( node.leaf == AST_LEAF_INDEX );
    assert( node.leaf_index().index == AST_INVALID_INDEX );

    // If there were no new upvals in the block, then there's nothing to do.
    assert( close_index <= upstack->upstack_slots.size() );
    if ( close_index >= upstack->upstack_slots.size() )
    {
        return;
    }

    // Close upstack.
    upstack->upstack_slots.resize( close_index );
    node.leaf_index().index = close_index;

    // If the entire upstack has been closed, then we can throw away all our
    // bookkeeping, its as if we start again (or its the end of the function).
    if ( close_index == 0 )
    {
        assert( upstack->upstack_slots.empty() );
        upstack->upstack_close.clear();
        return;
    }

    // Add new block close entry in case it needs to be updated later due to
    // an upstack slot being allocated underneath us.
    upstack->upstack_close.push_back( { block_index, close_index } );

    // Update the anchor index of all existing block close entries.
    for ( upstack_block& close : upstack->upstack_close )
    {
        close.floor_index = std::min( close.floor_index, close_index );
    }
}

}

