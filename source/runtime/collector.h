//
//  collector.h
//
//  Created by Edmund Kapusniak on 07/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_COLLECTOR_H
#define KF_COLLECTOR_H

/*
    GC happens like this:

        COLLECTOR                                   MUTATOR
                            -- orange epoch --
                                                    allocates orange objects
                                                    mutates references freely
                            -- safepoint --         reaches safepoint
                            mark roots
                            mark stacked cothreads
                            -- purple is live --
        mark reachable                              allocates purple objects
        grab mutator's marks                        old orange values are marked
        done marking                                eager mark cothreads
                            -- safepoint --         reaches safepoint
                            sweep orange keys
                            sweep orange layouts
                            -- orange is dead --
        sweep heap                                  all objects should be purple.
                            -- purple epoch --

    There are two colours, orange and purple.  At any point, the mutator:

      - Allocates objects as either orange or purple.
      - Marks old values which are either orange, purple, or neither.

    Marking and sweeping happens concurrently.  There are several operations
    that require synchronization.

    When GC is initiated:

      - The GC thread isn't doing anything except waiting to be triggered.
      - Marking of roots and active cothread stacks happens before resuming.

    During the mark phase:

      - The GC thread and the mutator thread have separate mark stacks.
      - When writing references, or when pushing a cothread, the mutator
        marks previously reachable objects that are unmarked.
      - When the GC thread runs out of work, it steals the mutator thread's
        mark stack.
      - When the GC thread steals an empty stack, marking is done.

    During the sweep phase:

      - The mutator must lock before allocating objects.
      - The GC thread must lock during sweeping.

    The mutator thread has a countdown of how many bytes its allowed to
    allocate from the heap before triggering a GC.  If this limit is exhausted,
    a GC is triggered at the next safepoint.
*/

#include "vmachine.h"

namespace kf
{

struct vmachine;
struct collector;

struct collector_statistics
{
    uint64_t tick_mark_pause;
    uint64_t tick_stack_pause;
    uint64_t tick_sweep_pause;
    uint64_t tick_heap_pause;
    size_t swept_bytes[ TYPE_COUNT ];
    size_t swept_count[ TYPE_COUNT ];
    size_t alive_bytes[ TYPE_COUNT ];
    size_t alive_count[ TYPE_COUNT ];
};

collector* collector_create();
void collector_destroy( collector* c );

void start_collector( vmachine* vm );
void stop_collector( vmachine* vm );

void add_stack_pause( collector* c, uint64_t tick );
void add_heap_pause( collector* c, uint64_t tick );

void safepoint( vmachine* vm );
void start_collection( vmachine* vm );
void wait_for_collection( vmachine* vm );

}

#endif

