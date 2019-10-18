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
#include <algorithm>
#include "ast.h"

namespace kf
{

/*
    The register allocation algorithm is described here.

    The live ranges of locals are constructed.  The live range of a local is
    a list of ops that define the variable, plus PHI/REF ops that import it
    into a block, in order.

    Each value (op result or local) which dies at a pinning instruction is
    *pinned*.  Register allocation for pinned instructions is delayed until
    the pinning instruction is allocated.

      - A pinning instruction is one of JUMP_RETURN, JUMP_FOR_SGEN, CALL,
        YCALL, YIELD, MOV, B_DEF, or B_PHI.  These instructions pin operands
        which have a live range that ends at the instruction.

    Values which are live *across* floated instructions are identified, and
    the number of live values across each floated instruction is counted.

      - A floated instruction is one of JUMP_RETURN, JUMP_FOR_SGEN, CALL,
        YCALL, or YIELD.  Floated instructions take multiple potentially
        pinned operands.

      - Floated instructions can themselves be pinned.

    Register allocation proceeds linearly.  Pinned instructions are skipped.
    The across count for each floated instruction is decremented each time an
    instruction is allocated.  Once it reaches zero, the pinned instruction
    is allocated, and the values it was pinning are unpinned and revisited to
    allocate them.

    A value is preferentially allocated to the following registers:

      - If it's a PARAM, register 1 + parameter index.

      - If it's a SELECT, the register it was generated from.

      - If it needs to be at the stack top (e.g. calls), the lowest register
        which has no registers allocated after it.

      - The register its pinning instruction would move it into.

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

    build_values();
    mark_pinning();
    allocate();
    debug_print_values();

    _value_locals.clear();
    _value_ranges.clear();
    _floated_ops.clear();
    _floated_across.clear();
    assert( _unpinned.empty() );
    _live_r.reset();
}

void ir_alloc::build_values()
{
    /*
        Build live ranges for each local by collecting each definition of it.
    */
    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        const ir_op* op = &_f->ops[ op_index ];
        if ( op->opcode == IR_REF || op->opcode == IR_PHI || op->local() == IR_INVALID_INDEX )
        {
            continue;
        }

        if ( op->opcode == IR_BLOCK )
        {
            const ir_block* block = &_f->blocks.at( _f->operands.at( op->oindex ).index );
            for ( unsigned phi_index = block->phi_head; phi_index != IR_INVALID_INDEX; )
            {
                const ir_op* phi = &_f->ops.at( phi_index );
                if ( phi->local() != IR_INVALID_LOCAL && phi->live_range != IR_INVALID_INDEX )
                {
                    _value_ranges.push_back( { phi->local(), op_index, phi->live_range } );
                }
                phi_index = phi->phi_next;
            }

            continue;
        }

        if ( op->local() != IR_INVALID_LOCAL && op->live_range != IR_INVALID_INDEX )
        {
            _value_ranges.push_back( { op->local(), op_index, op->live_range } );
        }
    }

    /*
        Sort live ranges.
    */
    std::sort
    (
        _value_ranges.begin(),
        _value_ranges.end(),
        []( const live_range& a, const live_range& b )
        {
            if ( a.local < b.local )
                return true;
            if ( a.local == b.local && a.lower < b.lower )
                return true;
            return false;
        }
    );

    /*
        Merge adjacent ranges.
    */
    unsigned next = 1;
    for ( unsigned live_index = 1; live_index < _value_ranges.size(); ++live_index )
    {
        live_range* pr = &_value_ranges[ next - 1 ];
        live_range* lr = &_value_ranges[ live_index ];

        if ( lr->lower >= lr->upper )
        {
            continue;
        }

        if ( pr->local == lr->local && pr->upper == lr->lower )
        {
            pr->upper = lr->upper;
            continue;
        }

        _value_ranges[ next ] = *lr;
        next += 1;
    }
    _value_ranges.resize( next );

    /*
        Build index.
    */
    for ( unsigned live_index = 0; live_index < _value_ranges.size(); )
    {
        unsigned local = _value_ranges[ live_index ].local;
        live_local value = { local, live_index, 0, IR_INVALID_INDEX, IR_INVALID_REGISTER, 0 };

        while ( live_index < _value_ranges.size() && _value_ranges[ live_index ].local == local )
        {
            value.live_count += 1;
            value.live_range = _value_ranges[ live_index ].upper;
            ++live_index;
        }

        _value_locals.push_back( value );
    }
}

void ir_alloc::mark_pinning()
{
    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        ir_op* op = &_f->ops[ op_index ];

        op->mark = 0;
        op->r = IR_INVALID_REGISTER;

        if ( op->live_range == IR_INVALID_INDEX )
        {
            continue;
        }

        if ( is_pinning( op ) )
        {
            /*
                Examine operands.  If they die at this op, then mark pinned.
            */
            for ( unsigned j = 0; j < op->ocount; ++j )
            {
                ir_operand operand = _f->operands.at( op->oindex + j );

                if ( operand.kind != IR_O_OP )
                    continue;

                if ( op->opcode == IR_B_DEF && j == 0 )
                    continue;

                ir_op* pinned_op = &_f->ops.at( operand.index );
                if ( pinned_op->local() == IR_INVALID_LOCAL )
                {
                    if ( pinned_op->live_range == op_index )
                    {
                        pinned_op->mark = 1;
                    }
                }
                else
                {
                    live_local* value = local_value( pinned_op->local() );
                    if ( value->live_range == op_index )
                    {
                        value->mark = 1;
                    }
                }
            }
        }

        if ( is_floated( op ) )
        {
            /*
                Scan block for all ops which are live across this op (i.e.
                live at the next op).  We only need to check this block,
                because ops that survive blocks will have a REF/PHI in the
                header giving their live range in this block.
            */

            unsigned floated_index = _floated_ops.size();
            floated_op floated = { op_index, 0 };

            unsigned check_index = op_index;
            while ( check_index-- )
            {
                const ir_op* check_op = &_f->ops[ check_index ];

                if ( check_op->opcode == IR_PHI || check_op->opcode == IR_REF )
                    continue;

                if ( check_op->opcode == IR_BLOCK )
                    break;

                if ( check_op->live_range != IR_INVALID_INDEX && check_op->live_range > op_index )
                {
                    _floated_across.emplace( check_index, floated_index );
                    floated.across_count += 1;
                }
            }

            const ir_op* block_op = &_f->ops.at( check_index );
            assert( block_op->opcode == IR_BLOCK );
            const ir_block* block = &_f->blocks.at( _f->operands.at( block_op->oindex ).index );

            unsigned phi_index = block->phi_head;
            while ( phi_index != IR_INVALID_INDEX )
            {
                ir_op* phi = &_f->ops.at( phi_index );

                if ( phi->live_range != IR_INVALID_INDEX && phi->live_range > op_index )
                {
                    _floated_across.emplace( check_index, floated_index );
                    floated.across_count += 1;
                }

                phi_index = phi->phi_next;
            }

            _floated_ops.push_back( floated );
        }
    }
}

void ir_alloc::allocate()
{
}

ir_alloc::live_local* ir_alloc::local_value( unsigned local_index )
{
    auto i = std::lower_bound
    (
        _value_locals.begin(),
        _value_locals.end(),
        local_index,
        []( const live_local& local_value, unsigned local_index )
        {
            return local_value.local_index < local_index;
        }
    );

    if ( i == _value_locals.end() || i->local_index != local_index )
    {
        assert( ! "missing local value" );
        return nullptr;
    }

    return &*i;
}

bool ir_alloc::is_pinning( const ir_op* op )
{
    switch ( op->opcode )
    {
    case IR_JUMP_RETURN:
    case IR_JUMP_FOR_SGEN:
    case IR_CALL:
    case IR_YCALL:
    case IR_YIELD:
    case IR_MOV:
    case IR_B_DEF:
    case IR_B_PHI:
        return true;

    default:
        return false;
    }
}

bool ir_alloc::is_floated( const ir_op* op )
{
    switch ( op->opcode )
    {
    case IR_JUMP_RETURN:
    case IR_JUMP_FOR_SGEN:
    case IR_CALL:
    case IR_YCALL:
    case IR_YIELD:
        return true;

    default:
        return false;
    }
}

void ir_alloc::debug_print_values()
{
    for ( unsigned i = 0; i < _value_locals.size(); ++i )
    {
        const live_local* local_value = &_value_locals[ i ];
        std::string_view name = _f->ast->locals.at( local_value->local_index ).name;
        printf( "VALUE %u ↓%04X",  local_value->local_index, local_value->live_range );

        if ( local_value->r != IR_INVALID_REGISTER )
            printf( " r%02u", local_value->r );
        else
            printf( "    " );

        if ( local_value->mark == IR_MARK_STICKY )
            printf( "@!" );
        else if ( local_value->mark )
            printf( "@%u", local_value->mark );
        else
            printf( "  " );

        printf( " %.*s\n", (int)name.size(), name.data() );

        for ( unsigned j = 0; j < local_value->live_count; ++j )
        {
            const live_range* local_range = &_value_ranges[ local_value->live_index + j ];
            printf( "  :%04X ↓%04X\n", local_range->lower, local_range->upper );
        }
    }
}

}

