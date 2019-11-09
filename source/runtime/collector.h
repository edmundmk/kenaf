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
        sweep heap                                  don't resurrect orange objects
        clear weakrefs                              all other objects should be purple
                            -- purple epoch --

    There are two colours, orange and purple.  At any point, the mutator:

      - Allocates objects as either orange or purple.
      - Marks old values which are either orange, purple, or neither.
      - Can resurrect objects of either colour or of only a single colour.

    Marking and sweeping happens concurrently.  There are several operations
    that require synchronization.

    When GC is initiated:

      - The GC thread isn't doing anything except waiting to be triggered.
      - Marking of roots and active cothread stacks happens before resuming.

    During the mark phase:

      - The GC thread and the mutator thread have separate mark stacks.
      - When writing references, or when pushing a cothread, the mutator
        marks previously reachable objects that are unmarked.
      - The mutator may freely resurrect objects of either colour.  Resurrected
        objects of the wrong colour must be marked.
      - When the GC thread runs out of work, it steals the mutator thread's
        mark stack.
      - When the GC thread steals an empty stack, marking is done.

    During the sweep phase:

      - The mutator must lock before resurrecting objects.  Objects of the dead
        colour are not resurrected, they must be re-created.
      - The mutator must lock before allocating objects.
      - The GC thread must lock before clearing weakrefs.
      - The GC thread must lock during sweeping.

    The mutator thread has a countdown of how many bytes its allowed to
    allocate from the heap before triggering a GC.  If this limit is exhausted,
    a GC is triggered at the next safepoint.
*/

namespace kf
{

struct vmachine;
struct collector;

collector* collector_create();
void collector_destroy( collector* c );

void safepoint( vmachine* vm );
void start_collection( vmachine* vm );
void wait_for_collection( vmachine* vm );

void sweep_entire_heap( vmachine* vm );

}

#endif

