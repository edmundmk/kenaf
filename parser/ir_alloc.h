//
//  ir_alloc.h
//
//  Created by Edmund Kapusniak on 15/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef IR_ALLOC_H
#define IR_ALLOC_H

/*
    Perform register allocation.  Registers are allocated to values in a
    fashion which attempts to both minimize unecessary moves and minimize the
    total number of registers used.
*/

#include "ir.h"
#include <vector>
#include <unordered_map>
#include <queue>

namespace kf
{

class ir_alloc
{
public:

    ir_alloc( source* source );
    ~ir_alloc();

    void alloc( ir_function* function );

private:

    struct live_r;

    struct live_index
    {
        unsigned local;         // local index.
        unsigned live_index;    // index in value_live_ranges.
        unsigned live_count;    // count of entries in value_live_ranges.
        unsigned pinned_by;     // index of instruction that pins this value.
    };

    struct live_range
    {
        unsigned local;         // local index.
        unsigned lower;         // instruction value becomes live/block start
        unsigned upper;         // instruction value dies/block end.
    };

    void build_values();
    void mark_pinning();

    bool is_pinning( const ir_op* op );
    bool is_stack_top( const ir_op* op );

    source* _source;
    ir_function* _f;

    std::vector< live_index > _value_index;    // instruction index -> value_live_ranges range
    std::vector< live_range > _value_ranges;   // individual live range.

    // instruction index -> call instruction index[]
    std::unordered_multimap< unsigned, unsigned > _live_across;

    // unpinned values in order of instruction index.
    std::priority_queue< unsigned, std::vector< unsigned >, std::greater< unsigned > > _unpinned;

    // stores where registers have been allocated.
    std::unique_ptr< live_r > _live_r;
};

}

#endif

