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

            lock H
            construct object
            unlock H

            During sweeping, we must lock to serialize access to the heap.

        SWEEPING

            while true
                lock H
                next/free heap
                unlock H
                check if object must be swept

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
static void gc_destroy( object* o );

static void sweep_entire_heap( vmachine* vm );

enum gc_state
{
    GC_STATE_WAIT,
    GC_STATE_MARK,
    GC_STATE_MARK_DONE,
    GC_STATE_SWEEP,
    GC_STATE_SWEEP_DONE,
    GC_STATE_QUIT,
};

struct collector
{
    collector();
    ~collector();

    // Synchronization.
    std::mutex work_mutex;
    std::condition_variable work_wait;

    // Mark list.
    segment_list< object* > mark_list;

    // State.
    gc_state state;
    gc_color white_color; // objects this colour are unmarked
    gc_color black_color; // object and all refs have been processed by marker.
    bool print_stats;

    // Sweep state.
    size_t heap_size;

    // GC thread.
    std::thread thread;

    // Statistics.
    collector_statistics statistics;
};

collector::collector()
    :   state( GC_STATE_WAIT )
    ,   white_color( GC_COLOR_ORANGE )
    ,   black_color( GC_COLOR_PURPLE )
    ,   print_stats( getenv( "KENAF_GC_PRINT_STATS" ) != nullptr )
    ,   heap_size( 0 )
    ,   statistics{}
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
        gc->state = GC_STATE_QUIT;
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

void add_heap_pause( collector* c, uint64_t tick )
{
    c->statistics.tick_heap_pause += tick;
}

void safepoint( vmachine* vm )
{
    if ( vm->countdown > 0 )
    {
        return;
    }

    if ( vm->phase == GC_PHASE_NONE )
    {
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
    collector* gc = vm->gc;

    if ( gc->state == GC_STATE_MARK_DONE )
    {
        assert( vm->phase == GC_PHASE_MARK );

        if ( ! vm->mark_list.empty() )
        {
            // Swap mark lists.
            assert( gc->mark_list.empty() );
            gc->mark_list.swap( vm->mark_list );
            assert( vm->mark_list.empty() );
            gc->state = GC_STATE_MARK;
            gc->work_wait.notify_all();
            return;
        }

        safepoint_start_sweep( vm );
    }
    else if ( gc->state == GC_STATE_SWEEP_DONE )
    {
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

    // Initialize phase.
    assert( gc->state == GC_STATE_WAIT );
    gc->state = GC_STATE_MARK;
    std::swap( gc->white_color, gc->black_color );

    assert( vm->phase == GC_PHASE_NONE );
    vm->phase = GC_PHASE_MARK;
    vm->old_color = gc->white_color;
    vm->new_color = gc->black_color;

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
    assert( gc->state == GC_STATE_MARK_DONE );
    gc->state = GC_STATE_SWEEP;
    gc->heap_size = 0;

    assert( vm->phase == GC_PHASE_MARK );
    vm->phase = GC_PHASE_SWEEP;
    vm->old_color = GC_COLOR_NONE;

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

const char* const TYPE_NAMES[ TYPE_COUNT ] =
{
    [ LOOKUP_OBJECT             ] = "lookup",
    [ STRING_OBJECT             ] = "string",
    [ ARRAY_OBJECT              ] = "array",
    [ TABLE_OBJECT              ] = "table",
    [ FUNCTION_OBJECT           ] = "function",
    [ NATIVE_FUNCTION_OBJECT    ] = "fnative",
    [ COTHREAD_OBJECT           ] = "cothread",
    [ NUMBER_OBJECT             ] = nullptr,
    [ BOOL_OBJECT               ] = nullptr,
    [ NULL_OBJECT               ] = nullptr,
    [ LAYOUT_OBJECT             ] = "layout",
    [ VSLOTS_OBJECT             ] = "vslots",
    [ KVSLOTS_OBJECT            ] = "kvslots",
    [ PROGRAM_OBJECT            ] = "program",
    [ SCRIPT_OBJECT             ] = "script",
};

void safepoint_new_epoch( vmachine* vm )
{
    collector* gc = vm->gc;

    // Report statistics.
    if ( gc->print_stats )
    {
        const collector_statistics& stats = gc->statistics;
        printf( "collection report:\n" );
        printf( "    mark  : %f\n", tick_seconds( stats.tick_mark_pause ) );
        printf( "    stack : %f\n", tick_seconds( stats.tick_stack_pause ) );
        printf( "    sweep : %f\n", tick_seconds( stats.tick_sweep_pause ) );
        printf( "    heap  : %f\n", tick_seconds( stats.tick_heap_pause ) );
        printf( "    swept :\n" );
        for ( size_t i = 0; i < TYPE_COUNT; ++i )
        {
            const char* name = TYPE_NAMES[ i ];
            if ( ! name ) continue;
            printf( "        %-8s : %zu / %zu bytes\n", name, stats.swept_count[ i ], stats.swept_bytes[ i ] );
        }
        printf( "    alive :\n" );
        for ( size_t i = 0; i < TYPE_COUNT; ++i )
        {
            const char* name = TYPE_NAMES[ i ];
            if ( ! name ) continue;
            printf( "        %-8s : %zu / %zu bytes\n", name, stats.alive_count[ i ], stats.alive_bytes[ i ] );
        }
    }

    // Update phase and allocation countdown.
    assert( gc->state == GC_STATE_SWEEP_DONE );
    gc->state = GC_STATE_WAIT;

    assert( vm->phase == GC_PHASE_SWEEP );
    vm->phase = GC_PHASE_NONE;
    vm->countdown = std::min< size_t >( std::max< size_t >( gc->heap_size / 2, 512 * 1024 ), UINT_MAX );
}

void gc_thread( vmachine* vm )
{
    collector* gc = vm->gc;
    std::unique_lock lock( gc->work_mutex );
    while ( true )
    {
        while ( gc->state == GC_STATE_WAIT || gc->state == GC_STATE_MARK_DONE || gc->state == GC_STATE_SWEEP_DONE )
        {
            gc->work_wait.wait( lock );
        }
        if ( gc->state == GC_STATE_MARK )
        {
            gc_mark( vm );
        }
        else if ( gc->state == GC_STATE_SWEEP )
        {
            gc_sweep( vm );
        }
        else if ( gc->state == GC_STATE_QUIT )
        {
            break;
        }
    }
}

void gc_mark( vmachine* vm )
{
    collector* gc = vm->gc;
    assert( gc->state == GC_STATE_MARK );

    gc_color black_color = gc->black_color;

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

        atomic_store( header( o )->color, black_color );
    }

    gc->state = GC_STATE_MARK_DONE;
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
    collector* gc = vm->gc;
    assert( gc->state == GC_STATE_SWEEP );
    gc_color white_color = gc->white_color;

    heap_state* heap = vm->heap;
    void* p = nullptr;
    bool free_chunk = false;

    // Process 'pages' of 64k each in the heap with the lock held.
    uintptr_t PAGE_MASK = ~(uintptr_t)( 64 * 1024 - 1 );
    uintptr_t page_base = 0;

    while ( true )
    {
        std::unique_lock lock_heap( vm->heap_mutex );
        while ( ( (uintptr_t)p & PAGE_MASK ) == page_base )
        {
            p = heap_sweep( heap, p, free_chunk );
            if ( ! p )
            {
                goto break_all;
            }

            type_code type = header( (object*)p )->type;
            gc_color color = (gc_color)atomic_load( header( (object*)p )->color );
            assert( color != GC_COLOR_MARKED );
            size_t size = heap_malloc_size( p );

            free_chunk = color == white_color;
            if ( ! free_chunk )
            {
                gc->heap_size += size;
                gc->statistics.alive_count[ type ] += 1;
                gc->statistics.alive_bytes[ type ] += size;
            }
            else
            {
                gc_destroy( (object*)p );
//              memset( p, 0xFE, size );
                gc->statistics.swept_count[ type ] += 1;
                gc->statistics.swept_bytes[ type ] += size;
            }
        }

        page_base = (uintptr_t)p & PAGE_MASK;
    }

break_all:
    gc->state = GC_STATE_SWEEP_DONE;
    return;
}

void gc_destroy( object* o )
{
    switch ( header( o )->type )
    {
    case LOOKUP_OBJECT:
        ( (lookup_object*)o )->~lookup_object();
        break;

    case STRING_OBJECT:
        ( (string_object*)o )->~string_object();
        break;

    case ARRAY_OBJECT:
        ( (array_object*)o )->~array_object();
        break;

    case TABLE_OBJECT:
        ( (table_object*)o )->~table_object();
        break;

    case FUNCTION_OBJECT:
        ( (function_object*)o )->~function_object();
        break;

    case NATIVE_FUNCTION_OBJECT:
        ( (native_function_object*)o )->~native_function_object();
        break;

    case COTHREAD_OBJECT:
        // This should be the only object type with a non-trivial destructor.
        ( (cothread_object*)o )->~cothread_object();
        break;

    case LAYOUT_OBJECT:
        ( (layout_object*)o )->~layout_object();
        break;

    case VSLOTS_OBJECT:
        ( (vslots_object*)o )->~vslots_object();
        break;

    case KVSLOTS_OBJECT:
        ( (kvslots_object*)o )->~kvslots_object();
        break;

    case PROGRAM_OBJECT:
        ( (program_object*)o )->~program_object();
        break;

    case SCRIPT_OBJECT:
        ( (script_object*)o )->~script_object();
        break;

    default: break;
    }
}

void sweep_entire_heap( vmachine* vm )
{
    heap_state* heap = vm->heap;
    void* p = nullptr;
    bool free_chunk = false;

    while ( true )
    {
        p = heap_sweep( heap, p, free_chunk );
        if ( ! p )
        {
            break;
        }

        free_chunk = atomic_load( header( (object*)p )->color ) != GC_COLOR_NONE;
        if ( free_chunk )
        {
            gc_destroy( (object*)p );
        }
    }
}

}

