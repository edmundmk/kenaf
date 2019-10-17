//
//  ir_alloc.cpp
//
//  Created by Edmund Kapusniak on 15/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ir_alloc.h"

namespace kf
{

/*
    The register allocation algorithm is described here.

    The live ranges of normal locals and of for loop implicit variables are
    constructed.  The live range of a normal variable is a list of ops that
    define the variable, plus PHI/REF ops that import it into a block, in
    order.  The live range of an implicit for loop variable is the entire
    range of the loop.

    A variable is identified with the first op that declares it.  For loop
    variables are identified with the JUMP_FOR_EACH or JUMP_FOR_STEP ops.

    Each value (instruction result, local, implicit for) has its live range
    examined.  Values which die at pinning instructions are *pinned*.  Register
    allocation for pinned instructions is delayed until the pinning instruction
    is allocated.

      - A pinning instruction is one of JUMP_RETURN, CALL, YCALL, YIELD,
        MOV, or B_PHI.  To pin a value, these instructions must use that value
        as an operand.

    Each stack top instruction is associated with the set of values which is
    live across it.

      - A stack top instruction is one of CALL, YCALL or YIELD.

      - Instructions VARARG, UNPACK, EXTEND, JUMP_FOR_EACH, JUMP_FOR_STEP, and
        FOR_EACH_ITEMS are also allocated at the stack top but they do not pin
        values, and so there is no advantage in allocating them early.

      - A stack top instruction can itself be pinned.

    A stack top instruction is register-allocated as soon as all values which
    are live across it are allocated.

    Then we do a modified linear scan, looking ahead to stack-top instructions
    once all stacked values have been allocated, and backtracking once values
    become unpinned.

    A value is preferentially allocated to the following registers:

      - If it's a parameter, register 1 + parameter index.

      - The register its pinning instruction needs it to be in.

      - The lowest numbered free register.

    Ranges where registers are allocated are tracked in a data structure which
    stores allocated live ranges for each register.
*/

/*
    This data structure holds the ranges at which each register is allocated.
    Currently this is very simple - an array of lists of allocated ranges.
    A more advanced data structure could potentially be more efficient.
*/

struct ir_alloc::live_r
{
    struct r_range
    {
        unsigned index : 31;
        unsigned alloc : 1;
    };

    typedef std::vector< r_range > r_range_list;
    std::vector< r_range_list > _r;

    live_r();
    ~live_r();

    bool check_register( unsigned r, const live_range* ranges, size_t rcount ) const;
    unsigned lowest_register( const live_range* ranges, size_t rcount ) const;
    unsigned stack_top( unsigned index ) const;
    void allocate_register( unsigned r, const live_range* ranges, size_t rcount );

};

ir_alloc::live_r::live_r()
{
}

ir_alloc::live_r::~live_r()
{
}

bool ir_alloc::live_r::check_register( unsigned r, const live_range* ranges, size_t rcount ) const
{
    if ( r >= _r.size() )
    {
        return true;
    }

    const r_range_list& rlist = _r.at( r );
    for ( size_t irange = 0; irange < rcount; ++irange )
    {
        // Search for allocation range containing the incoming range.
        // Should always find a valid range.
        const live_range& lr = ranges[ irange ];
        auto i = std::lower_bound( rlist.begin(), rlist.end(), lr.lower,
            []( const r_range& rr, unsigned lr_lower ) { return rr.index < lr_lower; } );

        // If this range is allocated, the incoming range interferes.
        if ( i->alloc )
        {
            return false;
        }

        // If this range is not allocated, the next range must be.
        ++i;
        assert( i->alloc );

        // If the next (allocated) range begins before the end of the incoming
        // range, then the incoming range interferes.
        if ( i->index < lr.upper )
        {
            return false;
        }
    }

    return true;
}

unsigned ir_alloc::live_r::lowest_register( const live_range* ranges, size_t rcount ) const
{
    for ( unsigned r = 0; r < _r.size(); ++r )
    {
        if ( check_register( r, ranges, rcount ) )
        {
            return r;
        }
    }
    return _r.size();
}

unsigned ir_alloc::live_r::stack_top( unsigned index ) const
{
    live_range range = { index, index + 1 };
    unsigned r = _r.size();
    while ( r && check_register( r - 1, &range, 1 ) )
    {
        r -= 1;
    }
    return r;
}

void ir_alloc::live_r::allocate_register( unsigned r, const live_range* ranges, size_t rcount )
{
    // Add ranges for registers if they don't exist.
    while ( r >= _r.size() )
    {
        _r.push_back( { { 0, false }, { 0x7FFFFFFF, true } } );
    }

    // Insert each live range one by one.
    r_range_list& rlist = _r.at( r );
    for ( size_t irange = 0; irange < rcount; ++irange )
    {
        // Find range containing incoming range.
        const live_range& lr = ranges[ irange ];
        auto i = std::lower_bound( rlist.begin(), rlist.end(), lr.lower,
            []( const r_range& rr, unsigned lr_lower ) { return rr.index < lr_lower; } );

        assert( ! i->alloc );
        if ( i->index != lr.lower )
        {
            // Split range, marking inserted range as allocated.
            i = rlist.insert( i + 1, { lr.lower, true } );
        }
        else
        {
            // Mark this range as allocated.
            i->alloc = true;
        }

        auto next = i; ++next;
        assert( next->alloc );
        if ( next->index > lr.upper )
        {
            // Split range again, marking inserted range as free.
            i = rlist.insert( i + 1, { lr.upper, true } );
        }
        else
        {
            // Merge i and next.  Do this by simply erasing next.
            rlist.erase( next );
        }
    }
}

ir_alloc::ir_alloc( source* source )
    :   _source( source )
{
}

ir_alloc::~ir_alloc()
{
}

void ir_alloc::alloc( ir_function* function )
{
    _f = function;
}

}

