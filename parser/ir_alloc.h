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
        unsigned live_index;    // index in value_ranges.
        unsigned live_count;    // count of entries in value_ranges.
        unsigned live_range;    // end of entire live range.
        uint8_t r;              // r
        bool mark;              // mark.
    };

    struct live_range
    {
        unsigned local_index;   // local index.
        unsigned lower;         // instruction value becomes live/block start.
        unsigned upper;         // instruction value dies/block end.
    };

    struct stacked
    {
        unsigned index;         // index of instruction.
        unsigned across_count;  // number of values live across this op.
    };

    void build_values();
    void mark_pinning();

    void allocate();
    void allocate_result( unsigned op_index, unsigned sweep_index );

    void anchor_stacked( unsigned stacked_index, unsigned sweep_index );
    void unpin_operands( unsigned op_index, unsigned sweep_index );

    bool is_stacked( const ir_op* op );
    bool is_pinning( const ir_op* op );

    void debug_print();

    source* _source;
    ir_function* _f;

    // Live ranges for local values, which have holes.
    std::vector< live_local > _local_values;
    std::vector< live_range > _local_ranges;

    // Stacked instructions and the values that are live across them.
    std::vector< stacked > _stacked;
    std::unordered_multimap< unsigned, unsigned > _stacked_across;

    // Unpinned values in order of instruction index.
    std::priority_queue< unsigned, std::vector< unsigned >, std::greater< unsigned > > _unpinned;

    // Stores ranges where registers have been allocated.
    std::unique_ptr< live_r > _live_r;
};

}

#endif

