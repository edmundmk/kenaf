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
#include "collector.h"
#include "call_stack.h"
#include "tick.h"
#include "objects/lookup_object.h"
#include "objects/cothread_object.h"

namespace kf
{

vcontext::vcontext()
    :   global_object( nullptr )
{
}

vcontext::~vcontext()
{
}

vmachine::vmachine()
    :   old_color( GC_COLOR_NONE )
    ,   new_color( GC_COLOR_PURPLE )
    ,   phase( GC_PHASE_NONE )
    ,   countdown( 512 * 1024 )
    ,   c( nullptr )
    ,   prototypes{}
    ,   self_key( nullptr )
    ,   self_sel{}
    ,   next_cookie( 0 )
    ,   context_list( nullptr )
    ,   heap( heap_create() )
    ,   gc( collector_create() )
{
}

vmachine::~vmachine()
{
    collector_destroy( gc );
    heap_destroy( heap );
}

void link_vcontext( vmachine* vm, vcontext* vc )
{
    assert( ! vc->next && ! vc->prev );
    vc->next = vm->context_list;
    vc->prev = nullptr;
    vm->context_list = vc;
}

void unlink_vcontext( vmachine* vm, vcontext* vc )
{
    if ( vc->next )
    {
        assert( vc->next->prev == vc );
        vc->next->prev = nullptr;
    }
    if ( vc->prev )
    {
        assert( vc->prev->next == vc );
        vc->prev->next = nullptr;
    }
    else
    {
        assert( vm->context_list == vc );
        vm->context_list = vc->next;
    }
    vc->next = nullptr;
    vc->prev = nullptr;
}

void destroy_vmachine( vmachine* vm )
{
    assert( ! vm->context_list );

    collector_destroy( vm->gc );
    vm->gc = nullptr;

    heap_destroy( vm->heap );
    vm->heap = nullptr;
}

/*
    Object allocation and write barriers.
*/

void* object_new( vmachine* vm, type_code type, size_t size )
{
    std::unique_lock lock_heap( vm->heap_mutex, std::defer_lock );
    if ( vm->phase == GC_PHASE_SWEEP )
    {
        uint64_t pause_start = tick();
        lock_heap.lock();
        add_heap_pause( vm->gc, tick() - pause_start );
    }

    // Allocate object from heap.
    void* p = heap_malloc( vm->heap, size );
    size = heap_malloc_size( p );
    vm->countdown -= std::min< size_t >( vm->countdown, size );

    // Initialize object header.
    object_header* h = header( (object*)p );
    atomic_store( h->color, vm->new_color );
    h->type = type;
    h->flags = 0;
    h->refcount = 0;

    // Fence so that consume reads of reference from GC thread get an
    // initialized object header with correct colour.
    atomic_produce_fence();
    if ( vm->phase == GC_PHASE_SWEEP )
    {
        lock_heap.unlock();
    }

    // Zero memory.
    memset( p, 0, size );
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

void write_barrier( vmachine* vm, value oldv )
{
    if ( box_is_object( oldv ) )
    {
        write_barrier( vm, unbox_object( oldv ) );
    }
    else
    {
        assert( box_is_string( oldv ) );
        write_barrier( vm, unbox_string( oldv ) );
    }
}

void write_barrier( vmachine* vm, object* old )
{
    // Add object to mark list.
    atomic_store( header( old )->color, GC_COLOR_MARKED );
    vm->mark_list.push_back( old );
}

void write_barrier( vmachine* vm, string_object* old )
{
    // Mark strings as mark colour since they have no references.
    atomic_store( header( old )->color, vm->new_color );
}

cothread_object* mark_cothread( vmachine* vm, cothread_object* cothread )
{
    // Mark entire cothread.  We mark references eagerly here rather than when
    // we write values into the stack in order to reduce the number of write
    // barriers required.
    if ( ! vm->old_color )
    {
        return cothread;
    }

    // Unlike other objects, check against new_color.  It doesn't matter if
    // the cothread has been pushed onto the mark list, we need it to be
    // completely marked before it can be used.

    // Must lock before marking because marked cothreads can be resized, which
    // would be disastrous if done while the GC is marking the cothread.
    // Either we mark, in which case the GC will not, or the GC has already
    // marked, in which case we will not.

    uint64_t pause_start = tick();
    std::lock_guard lock( vm->mark_mutex );
    add_stack_pause( vm->gc, tick() - pause_start );

    if ( atomic_load( header( cothread )->color ) == vm->new_color )
    {
        return cothread;
    }

    // Add all referenced objects to the mark list.
    for ( value v : cothread->stack )
    {
        write_barrier( vm, v );
    }

    // Add all functions in stack frames to the mark list.
    for ( const stack_frame& frame : cothread->stack_frames )
    {
        if ( frame.function )
        {
            write_barrier( vm, frame.function );
        }
    }

    // Mark with mark colour.
    atomic_store( header( cothread )->color, vm->new_color );
    return cothread;
}

/*
    Object model.
*/

static result object_self( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return return_void( frame );
}

void setup_object_model( vmachine* vm )
{
    // 'self' key.
    vm->self_key = string_key( vm, "self", 4 );

    // Root object.
    lookup_object* object = lookup_new( vm, nullptr );
    native_function_object* obself = native_function_new( vm, "object.self", object_self, nullptr, 1, 0 );
    lookup_setkey( vm, object, vm->self_key, &vm->self_sel, box_object( obself ) );

    // Prototype objects.
    vm->prototypes[ LOOKUP_OBJECT ] = object;
    vm->prototypes[ STRING_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ ARRAY_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ TABLE_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ FUNCTION_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ NATIVE_FUNCTION_OBJECT ] = vm->prototypes[ FUNCTION_OBJECT ];
    vm->prototypes[ COTHREAD_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ U64VAL_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ NUMBER_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ BOOL_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ NULL_OBJECT ] = lookup_new( vm, object );
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
    else if ( box_is_u64val( v ) )
    {
        return vm->prototypes[ U64VAL_OBJECT ];
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
    else if ( box_is_u64val( v ) )
    {
        return vm->prototypes[ U64VAL_OBJECT ];
    }
    else
    {
        return vm->prototypes[ NULL_OBJECT ];
    }
}

}

