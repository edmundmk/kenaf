//
//  collector.cpp
//
//  Created by Edmund Kapusniak on 07/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "collector.h"
#include "vmachine.h"

namespace kf
{

static void safepoint_handshake( vmachine* vm );
static void safepoint_start_mark( vmachine* vm );
static void safepoint_start_sweep( vmachine* vm );
static void safepoint_new_epoch( vmachine* vm );

static void mark( collector* gc, object* o );

struct collector
{
    // Synchronization.
    std::mutex work_mutex;
    std::condition_variable work_wait;

    // Mark state.
    segment_list< object* > mark_list;
    gc_color white_color; // objects this colour are unmarked
    gc_color black_color; // object and all refs have been processed by marker.
};

collector* collector_create()
{
    return new collector;
}

void collector_destroy( collector* c )
{
    delete c;
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

void sweep_entire_heap( vmachine* vm )
{
    // TODO.
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
        }
        else
        {
            // Everything is marked, move to sweep phase.
            safepoint_start_sweep( vm );
        }

        // Wake GC thread.
        gc->work_wait.notify_all();
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

    // Determine colours for this mark phase.
    gc_color white_color = gc->white_color = vm->new_color;
    gc_color black_color = gc->black_color = white_color == GC_COLOR_PURPLE ? GC_COLOR_ORANGE : GC_COLOR_PURPLE;
    vm->old_color = white_color;
    vm->new_color = black_color;
    vm->dead_color = GC_COLOR_NONE;
    vm->phase = GC_PHASE_MARK;

    // Mark roots.


}

void safepoint_start_sweep( vmachine* vm )
{
}

void safepoint_new_epoch( vmachine* vm )
{
}


}

