//
//  collector.cpp
//
//  Created by Edmund Kapusniak on 07/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

/*
    GC/mutator interactions.

    DURING EPOCH

        GC is not running.

    DURING MARK PHASE

        Handshake ensures both threads start with the same view of memory.
        All objects are white.

        OBJECT CONSTRUCTION

            initialize black object : normal / relaxed
            produce fence
            write ref : relaxed

            Mutator allocates new black object.
            Coloring of new objects black is ordered before any references
            to new objects are written.
            This is enforced by a produce barrier in object_new().

        GC REF READING

            read ref : consume

            GC reads references using consume ordering, to ensure that new
            objects read as black.

        MARK OF STRING REF (GC THREAD/MUTATOR THREAD)

            write black : relaxed

            String is coloured black.  This write is relaxed.  This thread
            will not re-mark this string.  The other thread might not see
            the relaxed write, but all that will happen is that the object is
            re-marked.

        MARK OF OBJECT REF (GC THREAD/MUTATOR THREAD)

            write grey : relaxed
            push onto list

            Object is coloured grey, and pushed onto the mark list.  This
            write is relaxed.  This thread will not re-mark this object.  The
            other thread might not see the relaxed write, but all that will
            happen is that the object is re-marked.

        MARK OF OBJECT (GC THREAD)

            write black : relaxed
            mark references

            Object is coloured black, and all of the references from the object
            are marked.  The write to the colour is relaxed, meaning the main
            thread might colour the object grey and re-mark it.  The references
            will be a mix of references visible at the start of the mark phase,
            and updated references.

            But the only thread that modifies references is the mutator thread,
            and the write barrier ensures that all original references will be
            coloured.

            Any weak references to this object must be coloured.

        MARK OF COTHREAD (GC THREAD)

            lock
            write black
            mark references
            unlock

            When the GC thread starts to mark a cothread, it must lock the
            cothread until it's visited all references on the stack.  We have
            to prevent the mutator from using the cothread (particularly,
            resizing it) while we are marking.

        MARK OF COTHREAD (MUTATOR THREAD)

            lock
            write black
            mark references
            unlock

            Before the mutator thread uses a cothread, it must mark all
            references on the stack.   We lock to ensure that if the GC thread
            is marking the cothread, that it finishes before we start using the
            cothread.

    DURING SWEEP PHASE

        Handshake ensures both threads start with the same view of memory.

        OBJECT CONSTRUCTION

            lock L
            set waiting flag
            lock H
            clear waiting flag
            unlock L
            construct object
            unlock H

            During sweeping, we must lock to serialize access to the heap.
            We use two locks to ensure that

        SWEEPING

            while true
                lock L
                lock H
                unlock L
                while not waiting flag:
                    sweep some amount of objects
                unlock H

            Must lock to serialize access to the heap.
*/

#include "collector.h"
#include <thread>
#include "vmachine.h"
#include "call_stack.h"
#include "heap.h"
#include "tick.h"
#include "objects/array_object.h"
#include "objects/cothread_object.h"
#include "objects/string_object.h"
#include "objects/table_object.h"

namespace kf
{

static void safepoint_handshake( vmachine* vm );
static void safepoint_start_mark( vmachine* vm );
static void safepoint_start_sweep( vmachine* vm );
static void safepoint_new_epoch( vmachine* vm );

static void gc_thread( vmachine* vm );
static void gc_mark( vmachine* vm );
static void gc_mark_value( collector* gc, value v );
static void gc_mark_object_ref( collector* gc, object* o );
static void gc_mark_string_ref( collector* gc, string_object* s );
static void gc_mark_cothread( collector* gc, vmachine* vm, cothread_object* cothread );
static void gc_sweep( vmachine* vm );

static void sweep_entire_heap( vmachine* vm );

struct collector
{
    collector();
    ~collector();

    // Synchronization.
    std::mutex work_mutex;
    std::condition_variable work_wait;

    // Mark state.
    segment_list< object* > mark_list;
    gc_color white_color; // objects this colour are unmarked
    gc_color black_color; // object and all refs have been processed by marker.
    gc_phase phase;

    // Sweep state.
    size_t heap_size;

    // GC thread.
    std::thread thread;

    // Statistics.
    collector_statistics statistics;
};

collector::collector()
    :   white_color( GC_COLOR_NONE )
    ,   black_color( GC_COLOR_NONE )
    ,   heap_size( 0 )
{
}

collector::~collector()
{
}

collector* collector_create()
{
    return new collector();
}

void collector_destroy( collector* c )
{
    assert( ! c->thread.joinable() );
    delete c;
}

void start_collector( vmachine* vm )
{
    vm->gc->thread = std::thread( gc_thread, vm );
}

void stop_collector( vmachine* vm )
{
    collector* gc = vm->gc;

    // Wait for collection to complete.
    wait_for_collection( vm );

    // Request that collector quits, then wait for it to do so.
    {
        std::lock_guard lock( gc->work_mutex );
        gc->phase = GC_PHASE_QUIT;
        gc->work_wait.notify_all();
    }
    gc->thread.join();

    // Sweep entire heap to destroy live objects.
    sweep_entire_heap( vm );
}

void add_stack_pause( collector* c, uint64_t tick )
{
    c->statistics.tick_stack_pause += tick;
}

void safepoint( vmachine* vm )
{
    if ( vm->phase == GC_PHASE_NONE && vm->countdown > 0 )
    {
        return;
    }

    // If allocation countdown has expired, kick off GC.
    if ( vm->phase == GC_PHASE_NONE )
    {
        assert( vm->countdown == 0 );
        std::lock_guard lock( vm->gc->work_mutex );
        safepoint_start_mark( vm );
        return;
    }

    // Try to handshake with the GC, unless it's busy.
    std::unique_lock lock( vm->gc->work_mutex, std::defer_lock );
    if ( lock.try_lock() )
    {
        safepoint_handshake( vm );
    }
}

void start_collection( vmachine* vm )
{
    // If GC is not already started, start marking.
    if ( vm->phase == GC_PHASE_NONE )
    {
        std::lock_guard lock( vm->gc->work_mutex );
        safepoint_start_mark( vm );
    }
}

void wait_for_collection( vmachine* vm )
{
    while ( vm->phase != GC_PHASE_NONE )
    {
        std::lock_guard lock( vm->gc->work_mutex );
        safepoint_handshake( vm );
    }
}

void safepoint_handshake( vmachine* vm )
{
    if ( vm->phase == GC_PHASE_MARK )
    {
        collector* gc = vm->gc;

        if ( ! vm->mark_list.empty() )
        {
            // Swap mark lists.
            gc->mark_list.swap( vm->mark_list );
            assert( vm->mark_list.empty() );
            gc->work_wait.notify_all();
        }
        else
        {
            // Everything is marked, move to sweep phase.
            safepoint_start_sweep( vm );
        }
    }
    else
    {
        // Sweeping is done, move to new epoch.
        assert( vm->phase == GC_PHASE_SWEEP );
        safepoint_new_epoch( vm );
    }
}

void safepoint_start_mark( vmachine* vm )
{
    collector* gc = vm->gc;
    uint64_t pause_start = tick();

    // Reset statistics.
    gc->statistics = {};

    // Determine colours for this mark phase.
    gc_color white_color = gc->white_color = vm->new_color;
    gc_color black_color = gc->black_color = white_color == GC_COLOR_PURPLE ? GC_COLOR_ORANGE : GC_COLOR_PURPLE;
    vm->old_color = white_color;
    vm->new_color = black_color;
    vm->phase = gc->phase = GC_PHASE_MARK;

    // Add all roots to mark list.
    assert( gc->mark_list.empty() );

    gc_mark_string_ref( gc, vm->self_key );

    for ( const auto& root : vm->roots )
    {
        object* o = root.first;
        if ( header( o )->type != STRING_OBJECT )
        {
            gc_mark_object_ref( gc, o );
        }
        else
        {
            gc_mark_string_ref( gc, (string_object*)o );
        }
    }

    for ( vcontext* c = vm->context_list; c; c = c->next )
    {
        gc_mark_object_ref( gc, c->global_object );
        for ( cothread_object* cothread : c->cothread_stack )
        {
            gc_mark_object_ref( gc, cothread );
        }
        if ( c != vm->c )
        {
            gc_mark_object_ref( gc, c->cothread );
        }
    }

    gc_mark_cothread( gc, vm, vm->c->cothread );

    // Signal GC thread.
    gc->work_wait.notify_all();

    // Set pause time.
    gc->statistics.tick_mark_pause = tick() - pause_start;
}

void safepoint_start_sweep( vmachine* vm )
{
    collector* gc = vm->gc;
    uint64_t pause_start = tick();

    // Disable write barrier and update phase.
    vm->old_color = GC_COLOR_NONE;
    vm->phase = gc->phase = GC_PHASE_SWEEP;
    gc->heap_size = 0;

    // Get colour for dead objects.
    gc_color dead_color = gc->white_color;

    // Clear references to dead keys and layouts.
    auto keys_end = vm->keys.end();
    for ( auto i = vm->keys.begin(); i != keys_end; )
    {
        if ( atomic_load( header( i->second )->color ) == dead_color )
        {
            i = vm->keys.erase( i );
        }
        else
        {
            ++i;
        }
    }

    auto instance_layouts_end = vm->instance_layouts.end();
    for ( auto i = vm->instance_layouts.begin(); i != instance_layouts_end; )
    {
        if ( atomic_load( header( i->second )->color ) == dead_color )
        {
            i = vm->instance_layouts.erase( i );
            continue;
        }

        // Search down next list and prune any dead layout.
        layout_object* layout = i->second;
        while ( layout_object* next = layout->next )
        {
            if ( atomic_load( header( next )->color ) == dead_color )
            {
                layout->next = nullptr;
                break;
            }
            layout = next;
        }

        ++i;
    }

    auto splitkey_layouts_end = vm->splitkey_layouts.end();
    for ( auto i = vm->splitkey_layouts.begin(); i != splitkey_layouts_end; )
    {
        if ( atomic_load( header( i->second )->color ) == dead_color )
        {
            i = vm->splitkey_layouts.erase( i );
            continue;
        }

        // Search down next list and prune any dead layout.
        layout_object* layout = i->second;
        while ( layout_object* next = layout->next )
        {
            if ( atomic_load( header( next )->color ) == dead_color )
            {
                layout->next = nullptr;
                break;
            }
            layout = next;
        }

        ++i;
    }

    // Signal GC thread.
    gc->work_wait.notify_all();

    // Set pause time.
    gc->statistics.tick_sweep_pause = tick() - pause_start;
}

void safepoint_new_epoch( vmachine* vm )
{
    collector* gc = vm->gc;

    // Report statistics.
    printf( "collection report:\n" );
    printf( "    mark  : %f\n", tick_seconds( gc->statistics.tick_mark_pause ) );
    printf( "    stack : %f\n", tick_seconds( gc->statistics.tick_stack_pause ) );
    printf( "    sweep : %f\n", tick_seconds( gc->statistics.tick_sweep_pause ) );

    // Update phase and allocation countdown.
    vm->phase = gc->phase = GC_PHASE_NONE;
    vm->countdown = std::max< size_t >( std::min< size_t >( gc->heap_size / 2, 512 * 1024 ), UINT_MAX );
}

void gc_thread( vmachine* vm )
{
    collector* gc = vm->gc;
    while ( true )
    {
        std::unique_lock lock( gc->work_mutex );
        gc->work_wait.wait( lock );
        if ( gc->phase == GC_PHASE_MARK )
        {
            gc_mark( vm );
        }
        else if ( gc->phase == GC_PHASE_SWEEP )
        {
            gc_sweep( vm );
        }
        else if ( gc->phase == GC_PHASE_QUIT )
        {
            break;
        }
    }
}

void gc_mark( vmachine* vm )
{
    collector* gc = vm->gc;

    // Mark until work list is empty.
    while ( ! gc->mark_list.empty() )
    {
        object* o = gc->mark_list.back();
        gc->mark_list.pop_back();

        switch ( header( o )->type )
        {
        case LOOKUP_OBJECT:
        {
            lookup_object* lookup = (lookup_object*)o;
            gc_mark_object_ref( gc, atomic_consume( lookup->oslots ) );
            gc_mark_object_ref( gc, atomic_consume( lookup->layout ) );
            break;
        }

        case STRING_OBJECT:
        {
            assert( ! "string object should not be on mark list" );
            break;
        }

        case ARRAY_OBJECT:
        {
            array_object* array = (array_object*)o;
            gc_mark_object_ref( gc, atomic_consume( array->aslots ) );
            break;
        }

        case TABLE_OBJECT:
        {
            table_object* table = (table_object*)o;
            gc_mark_object_ref( gc, atomic_consume( table->kvslots ) );
            break;
        }

        case FUNCTION_OBJECT:
        {
            function_object* function = (function_object*)o;
            gc_mark_object_ref( gc, atomic_consume( function->program ) );
            gc_mark_object_ref( gc, atomic_consume( function->omethod ) );
            size_t count = ( heap_malloc_size( function ) - offsetof( function_object, outenvs ) ) / sizeof( ref< vslots_object > );
            for ( size_t i = 0; i < count; ++i )
            {
                gc_mark_object_ref( gc, atomic_consume( function->outenvs[ i ] ) );
            }
            break;
        }

        case NATIVE_FUNCTION_OBJECT:
        {
            break;
        }

        case COTHREAD_OBJECT:
        {
            cothread_object* cothread = (cothread_object*)o;
            gc_mark_cothread( gc, vm, cothread );
            break;
        }

        case LAYOUT_OBJECT:
        {
            layout_object* layout = (layout_object*)o;
            gc_mark_object_ref( gc, atomic_consume( layout->parent ) );
            gc_mark_string_ref( gc, atomic_consume( layout->key ) );
            break;
        }

        case VSLOTS_OBJECT:
        {
            vslots_object* vslots = (vslots_object*)o;
            size_t count = heap_malloc_size( vslots ) / sizeof( ref_value );
            for ( size_t i = 0; i < count; ++i )
            {
                gc_mark_value( gc, { atomic_consume( vslots->slots[ i ] ) } );
            }
            break;
        }

        case KVSLOTS_OBJECT:
        {
            kvslots_object* kvslots = (kvslots_object*)o;
            size_t count = kvslots->count;
            for ( size_t i = 0; i < count; ++i )
            {
                const kvslot* kv = kvslots->slots + i;
                gc_mark_value( gc, { atomic_consume( kv->k ) } );
                gc_mark_value( gc, { atomic_consume( kv->v ) } );
            }
            break;
        }

        case PROGRAM_OBJECT:
        {
            program_object* program = (program_object*)o;
            gc_mark_object_ref( gc, atomic_consume( program->script ) );
            size_t count = program->constant_count;
            for ( size_t i = 0; i < count; ++i )
            {
                gc_mark_value( gc, { atomic_consume( program->constants[ i ] ) } );
            }
            count = program->selector_count;
            for ( size_t i = 0; i < count; ++i )
            {
                gc_mark_string_ref( gc, atomic_consume( program->selectors[ i ].key ) );
            }
            count = program->function_count;
            for ( size_t i = 0; i < count; ++i )
            {
                gc_mark_object_ref( gc, atomic_consume( program->functions[ i ] ) );
            }
            break;
        }

        case SCRIPT_OBJECT:
        {
            break;
        }

        default: break;
        }
    }
}

void gc_mark_value( collector* gc, value v )
{
    if ( ! box_is_object_or_string( v ) )
    {
        return;
    }

    object* o = unbox_object_or_string( v );
    if ( atomic_load( header( o )->color ) != gc->white_color )
    {
        return;
    }

    if ( box_is_object( v ) )
    {
        atomic_store( header( o )->color, GC_COLOR_MARKED );
        gc->mark_list.push_back( o );
    }
    else
    {
        assert( box_is_string( v ) );
        atomic_store( header( o )->color, gc->black_color );
    }
}

void gc_mark_object_ref( collector* gc, object* o )
{
    if ( o && atomic_load( header( o )->color ) == gc->white_color )
    {
        atomic_store( header( o )->color, GC_COLOR_MARKED );
        gc->mark_list.push_back( o );
    }
}

void gc_mark_string_ref( collector* gc, string_object* o )
{
    if ( o && atomic_load( header( o )->color ) == gc->white_color )
    {
        atomic_store( header( o )->color, gc->black_color );
    }
}

void gc_mark_cothread( collector* gc, vmachine* vm, cothread_object* cothread )
{
    // Lock to synchronize with mutator thread.
    std::lock_guard lock( vm->mark_mutex );

    // Don't re-mark if the cothread is black already.
    if ( atomic_load( header( cothread )->color ) == gc->black_color )
    {
        return;
    }

    // Mark all stack references.
    for ( value v : cothread->stack )
    {
        gc_mark_value( gc, v );
    }

    // Mark all references to functions in call stack.
    for ( const stack_frame& frame : cothread->stack_frames )
    {
        gc_mark_object_ref( gc, frame.function );
    }

    // Mark.
    atomic_store( header( cothread )->color, gc->black_color );
}

void gc_sweep( vmachine* vm )
{
}

void sweep_entire_heap( vmachine* vm )
{
}

}

