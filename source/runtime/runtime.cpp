//
//  runtime.cpp
//
//  Created by Edmund Kapusniak on 01/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "kenaf/kenaf.h"
#include "vm/vm_context.h"

namespace kf
{

/*
    Thread-local VM state.
*/

static thread_local runtime* current_runtime;

/*
    Runtime and context.
*/

struct runtime
{
    vm_context vm;
    intptr_t refcount;
    context* current_context;
};

struct context
{
    intptr_t refcount;
    struct runtime* runtime;
    std::vector< cothread_object* > cothreads;
    lookup_object* global_object;
};

runtime* create_runtime()
{
    runtime* r = new runtime();
    r->refcount = 1;
    r->current_context = nullptr;
    return r;
}

runtime* retain_runtime( runtime* r )
{
    assert( r->refcount >= 1 );
    r->refcount += 1;
    return r;
}

void release_runtime( runtime* r )
{
    assert( r->refcount >= 1 );
    if ( --r->refcount == 0 )
    {
        if ( current_runtime == r )
        {
            make_current( nullptr );
        }
        delete r;
    }
}

context* create_context( runtime* r )
{
    context* c = new context();
    c->refcount = 1;
    c->runtime = r;
    c->global_object = lookup_new( &r->vm, r->vm.prototypes[ LOOKUP_OBJECT ] );
    return c;
}

context* retain_context( context* c )
{
    assert( c->refcount >= 1 );
    c->refcount += 1;
    return c;
}

void release_context( context* c )
{
    assert( c->refcount >= 1 );
    if ( --c->refcount == 0 )
    {
        if ( current_runtime == c->runtime && c->runtime->current_context == c )
        {
            make_current( nullptr );
        }
        delete c;
    }
}

context* make_current( context* c )
{
    context* prev = nullptr;

    if ( current_runtime )
    {
        prev = current_runtime->current_context;
        std::swap( current_runtime->vm.cothreads, prev->cothreads );
        assert( current_runtime->vm.global_object == prev->global_object );
        current_runtime->vm.global_object = nullptr;
        current_runtime->current_context = nullptr;
        current_runtime = nullptr;
    }

    if ( c )
    {
        current_runtime = c->runtime;
        std::swap( current_runtime->vm.cothreads, c->cothreads );
        current_runtime->vm.global_object = c->global_object;
        current_runtime->current_context = c;
    }

    return prev;
}

value global_object()
{
    return box_object( current_runtime->vm.global_object );
}

/*
    Public API.
*/

inline vm_context* current() { return &current_runtime->vm; }

value retain( value v )
{
    // TODO
    return v;
}

void release( value v )
{
    // TODO
}

value_kind kindof( value v )
{
    if ( box_is_number( v ) )
    {
        return NUMBER;
    }
    else if ( box_is_string( v ) )
    {
        return STRING;
    }
    else if ( box_is_object( v ) )
    {
        return (value_kind)( header( unbox_object( v ) )->type );
    }
    else if ( box_is_bool( v ) )
    {
        return BOOL_VALUE;
    }
    else
    {
        return NULL_VALUE;
    }
}

bool is_lookup( value v )
{
    return box_is_object( v ) && header( unbox_object( v ) )->type == LOOKUP_OBJECT;
}

bool is_string( value v )
{
    return box_is_string( v );
}

bool is_array( value v )
{
    return box_is_object( v ) && header( unbox_object( v ) )->type == ARRAY_OBJECT;
}

bool is_table( value v )
{
    return box_is_object( v ) && header( unbox_object( v ) )->type == TABLE_OBJECT;
}

bool is_function( value v )
{
    return box_is_object( v ) && header( unbox_object( v ) )->type == FUNCTION_OBJECT;
}

bool is_cothread( value v )
{
    return box_is_object( v ) && header( unbox_object( v ) )->type == COTHREAD_OBJECT;
}

bool is_number( value v )
{
    return box_is_number( v );
}

bool is_bool( value v )
{
    return box_is_bool( v );
}

bool is_null( value v )
{
    return box_is_null( v );
}

value null_value()
{
    return boxed_null;
}

value true_value()
{
    return boxed_true;
}

value false_value()
{
    return boxed_false;
}

value superof( value v )
{
    vm_context* vm = current();
    if ( box_is_number( v ) )
    {
        return box_object( vm->prototypes[ NUMBER_OBJECT ] );
    }
    else if ( box_is_string( v ) )
    {
        return box_object( vm->prototypes[ STRING_OBJECT ] );
    }
    else if ( box_is_object( v ) )
    {
        type_code type = header( unbox_object( v ) )->type;
        if ( type == LOOKUP_OBJECT )
        {
            return box_object( lookup_prototype( vm, (lookup_object*)unbox_object( v ) ) );
        }
        else
        {
            return box_object( vm->prototypes[ type ] );
        }
    }
    else if ( box_is_bool( v ) )
    {
        return box_object( vm->prototypes[ BOOL_OBJECT ] );
    }
    else
    {
        return box_object( vm->prototypes[ NULL_OBJECT ] );
    }
}

bool test( value v )
{
    return v.v > 1 && v.v != box_number( +0.0 ).v && v.v != box_number( -0.0 ).v;
}

value number_value( double n )
{
    return box_number( n );
}

double get_number( value v )
{
    if ( ! is_number( v ) ) throw std::exception();
    return unbox_number( v );
}





}

