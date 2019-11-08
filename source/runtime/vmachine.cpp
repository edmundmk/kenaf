//
//  vmachine.cpp
//
//  Created by Edmund Kapusniak on 25/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "vmachine.h"
#include <stdlib.h>
#include "heap.h"
#include "objects/lookup_object.h"

namespace kf
{

vmachine::vmachine()
    :   old_color( GC_COLOR_NONE )
    ,   new_color( GC_COLOR_PURPLE )
    ,   dead_color( GC_COLOR_NONE )
    ,   heap( heap_create() )
    ,   cothreads( nullptr )
    ,   global_object( nullptr )
    ,   prototypes{}
    ,   self_key( nullptr )
    ,   self_sel{}
    ,   next_cookie( 0 )
{
}

vmachine::~vmachine()
{
    heap_destroy( heap );
}

/*
    Object allocation and write barriers.
*/

void* object_new( vmachine* vm, type_code type, size_t size )
{
    void* p = heap_malloc( vm->heap, size );
    object_header* h = header( (object*)p );
    atomic_store( h->color, vm->new_color );
    h->type = type;
    h->flags = 0;
    h->refcount = 0;
    atomic_produce_fence();
    return p;
}

size_t object_size( vmachine* vm, object* object )
{
    return heap_malloc_size( object );
}

void object_retain( vmachine* vm, object* object )
{
    object_header* h = header( object );
    if ( h->refcount == 0 )
    {
        assert( ! vm->roots.contains( object ) );
        vm->roots.insert_or_assign( object, 0 );
    }
    if ( ++h->refcount == 0 )
    {
        vm->roots.at( object ) += 1;
        h->refcount = 255;
    }
}

void object_release( vmachine* vm, object* object )
{
    object_header* h = header( object );
    assert( h->refcount > 0 );
    if ( h->refcount == 255 )
    {
        size_t& refcount = vm->roots.at( object );
        if ( refcount > 0 )
        {
            refcount -= 1;
        }
        else
        {
            h->refcount -= 1;
        }
    }
    else if ( --h->refcount == 0 )
    {
        vm->roots.erase( object );
    }
}

void write_barrier( vmachine* vm, object* old )
{
    // TODO.
}

/*
    Object model.
*/

void setup_object_model( vmachine* vm )
{
    // Prototype objects.
    lookup_object* object = lookup_new( vm, nullptr );
    vm->prototypes[ LOOKUP_OBJECT ] = object;
    vm->prototypes[ STRING_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ ARRAY_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ TABLE_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ FUNCTION_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ NATIVE_FUNCTION_OBJECT ] = vm->prototypes[ FUNCTION_OBJECT ];
    vm->prototypes[ COTHREAD_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ NUMBER_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ BOOL_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ NULL_OBJECT ] = lookup_new( vm, object );

    // 'self' key.
    vm->self_key = string_key( vm, "self", 4 );
}

lookup_object* value_keyerof( vmachine* vm, value v )
{
    if ( box_is_number( v ) )
    {
        return vm->prototypes[ NUMBER_OBJECT ];
    }
    else if ( box_is_string( v ) )
    {
        return vm->prototypes[ STRING_OBJECT ];
    }
    else if ( box_is_object( v ) )
    {
        type_code type = header( unbox_object( v ) )->type;
        if ( type == LOOKUP_OBJECT )
        {
            return (lookup_object*)unbox_object( v );
        }
        else
        {
            return vm->prototypes[ type ];
        }
    }
    else if ( box_is_bool( v ) )
    {
        return vm->prototypes[ BOOL_OBJECT ];
    }
    else
    {
        return vm->prototypes[ NULL_OBJECT ];
    }
}

lookup_object* value_superof( vmachine* vm, value v )
{
    if ( box_is_number( v ) )
    {
        return vm->prototypes[ NUMBER_OBJECT ];
    }
    else if ( box_is_string( v ) )
    {

        return vm->prototypes[ STRING_OBJECT ];
    }
    else if ( box_is_object( v ) )
    {
        type_code type = header( unbox_object( v ) )->type;
        if ( type == LOOKUP_OBJECT )
        {
            return lookup_prototype( vm, (lookup_object*)unbox_object( v ) );
        }
        else
        {
            return vm->prototypes[ type ];
        }
    }
    else if ( box_is_bool( v ) )
    {
        return vm->prototypes[ BOOL_OBJECT ];
    }
    else
    {
        return vm->prototypes[ NULL_OBJECT ];
    }
}

}

