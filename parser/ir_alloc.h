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

    struct live_local
    {
        unsigned local_index;   // local index.
        unsigned live_index;    // index in value_ranges.
        unsigned live_count;    // count of entries in value_ranges.
        unsigned live_range;    // end of entire live range.
        uint8_t r;              // r
        bool mark;              // mark.
    };

    struct live_range
    {
        unsigned local;         // local index.
        unsigned lower;         // instruction value becomes live/block start.
        unsigned upper;         // instruction value dies/block end.
    };

    struct floated_op
    {
        unsigned index;         // index of op.
        unsigned across_count;  // number of values live across this op.
    };

    void build_values();
    void mark_pinning();
    void allocate();

    live_local* local_value( unsigned local_index );
    bool is_pinning( const ir_op* op );
    bool is_floated( const ir_op* op );

    void debug_print_values();

    source* _source;
    ir_function* _f;

    // live ranges for local values, which have holes.
    std::vector< live_local > _value_locals;
    std::vector< live_range > _value_ranges;

    // floated instructions and the values that are live across them.
    std::vector< floated_op > _floated_ops;
    std::unordered_multimap< unsigned, unsigned > _floated_across;

    // unpinned values in order of instruction index.
    std::priority_queue< unsigned, std::vector< unsigned >, std::greater< unsigned > > _unpinned;

    // stores where registers have been allocated.
    std::unique_ptr< live_r > _live_r;
};

}

#endif

