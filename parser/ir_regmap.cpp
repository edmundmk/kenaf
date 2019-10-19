//
//  ir_regmap.cpp
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ir_regmap.h"
#include "ir.h"

namespace kf
{

ir_regmap::ir_regmap()
{
}

ir_regmap::~ir_regmap()
{
}

bool ir_regmap::check( unsigned r, const ir_value_range* ranges, size_t rcount ) const
{
    if ( r >= _rr.size() )
    {
        return true;
    }

    const reg_range_list& rlist = _rr.at( r );
    for ( size_t irange = 0; irange < rcount; ++irange )
    {
        const ir_value_range& vr = ranges[ irange ];
        if ( vr.lower >= vr.upper )
            continue;

        // Search for allocation range containing the incoming range.
        auto i = --std::upper_bound( rlist.begin(), rlist.end(), vr.lower,
            []( unsigned vr_lower, const reg_range& rr ) { return vr_lower < rr.index; } );

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
        if ( i->index < vr.upper )
        {
            return false;
        }
    }

    return true;
}

unsigned ir_regmap::lowest( const ir_value_range* ranges, size_t rcount ) const
{
    for ( unsigned r = 0; r < _rr.size(); ++r )
    {
        if ( check( r, ranges, rcount ) )
        {
            return r;
        }
    }
    return _rr.size();
}

unsigned ir_regmap::top( unsigned index ) const
{
    unsigned r = _rr.size();
    ir_value_range range = { IR_INVALID_LOCAL, index, index + 1 };
    while ( r && check( r - 1, &range, 1 ) )
    {
        r -= 1;
    }
    return r;
}

void ir_regmap::allocate( unsigned r, const ir_value_range* ranges, size_t rcount )
{
    // Add ranges for registers if they don't exist.
    while ( r >= _rr.size() )
    {
        _rr.push_back( { { 0, false }, { 0x7FFFFFFF, true } } );
    }

    // Insert each live range one by one.
    reg_range_list& rlist = _rr.at( r );
    for ( size_t irange = 0; irange < rcount; ++irange )
    {
        const ir_value_range& vr = ranges[ irange ];
        if ( vr.lower >= vr.upper )
            continue;

        // Find range containing incoming range.
        auto i = --std::upper_bound( rlist.begin(), rlist.end(), vr.lower,
            []( unsigned vr_lower, const reg_range& rr ) { return vr_lower < rr.index; } );

        assert( ! i->alloc );
        if ( i->index != vr.lower )
        {
            // Split range, marking inserted range as allocated.
            i = rlist.insert( i + 1, { vr.lower, true } );
        }
        else
        {
            // Mark this range as allocated.
            i->alloc = true;
        }

        auto next = i; ++next;
        assert( next->alloc );
        if ( next->index > vr.upper )
        {
            // Split range again, marking inserted range as free.
            i = rlist.insert( i + 1, { vr.upper, false } );
        }
        else
        {
            // Merge i and next.  Do this by simply erasing next.
            rlist.erase( next );
        }
    }
}

void ir_regmap::clear()
{
    for ( unsigned r = 0; r < _rr.size(); ++r )
    {
        _rr[ r ].assign( { { 0, false }, { 0x7FFFFFFF, true } } );
    }
}

void ir_regmap::debug_print() const
{
    for ( unsigned r = 0; r < _rr.size(); ++r )
    {
        printf( "  r%u :: ", r );

        const reg_range_list& rlist = _rr[ r ];
        for ( size_t i = 0; i < rlist.size(); ++i )
        {
            const reg_range& rr = rlist[ i ];
            printf( "%s%04X", rr.alloc ? "," : ":", rr.index );
        }

        printf( "\n" );
    }
}

}

