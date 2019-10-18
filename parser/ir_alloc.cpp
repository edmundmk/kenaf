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
    An instruction has two register numbers that we need to determine.

    The result register, r, contains the result of the operation.  All
    instructions produce a result except:

      - SET_KEY, SET_INDEX, SET_ENV, APPEND, EXTEND, AND, CUT, BLOCK, and all
        JUMP instructions.
      - CALL, YCALL, YIELD, VARARG, UNPACK, and FOR_EACH_ITEMS with an unpack
        count greater than one, in which case either:
          - Results are assigned to registers by following SELECT instructions.
          - The entire result list is the last argument to a following CALL,
            YCALL, YIELD, EXTEND, or JUMP_RETURN instruction.

    The stack top register, s, is required by instructions which consume or
    produce more than one value in adjacent registers.  This is:

      - Call/return instructions CALL, YCALL, and YIELD.
      - VARARG.
      - Array UNPACK and EXTEND.
      - JUMP_RETURN
      - JUMP_FOR_EGEN and JUMP_FOR_SGEN produce adjacent for hidden variables.
      - FOR_EACH_ITEMS generates a list of items.

    The two registers are not necessarily related.  We can always shuffle
    single argument and result values into the required registers using moves,
    and stack top of instructions producing result lists can always be slid
    rightwards to a higher register number.

    But our register allocation algorithm attempts to minimize the number of
    move instructions.  We do this by identifying *pinned* values.

    A pinned value is a value which dies at its use as an operand of a
    *pinning* instruction.  A pinning instruction is either:

      - An instruction which requires a stack top register, and which consumes
        more than one value.  This is CALL, YCALL, YIELD, EXTEND, JUMP_RETURN,
        and JUMP_FOR_EGEN.
      - An instruction which passes through its operand unchanged, i.e. MOV,
        B_DEF, or B_PHI.

    Our register allocator is greedy.  Once a register has been allocated to a
    value, we never backtrack.

    We allocate the r register of values in program order, based on the index
    of their first definition.  Pinned values are skipped.

    As soon as all values live across a stacked instruction are allocated,
    the stack top register s for that instruction can be determined.  We do
    this immediately, no matter where we are in program order.  This is called
    anchoring.  All operands to the anchored instruction are unpinned.

    When a MOV, B_DEF, or B_PHI instruction is allocated, its operand value is
    unpinned.

    Unpinned values are allocated intermixed with other values, in program
    order.
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
    live_range range = { IR_INVALID_LOCAL, index, index + 1 };
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
    _live_r = std::make_unique< live_r >();

    build_values();
    mark_pinning();
    allocate();
    debug_print();

    _local_values.clear();
    _local_ranges.clear();
    _stacked.clear();
    _stacked_across.clear();
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
                    _local_ranges.push_back( { phi->local(), op_index, phi->live_range } );
                }
                phi_index = phi->phi_next;
            }

            continue;
        }

        if ( op->local() != IR_INVALID_LOCAL && op->live_range != IR_INVALID_INDEX )
        {
            _local_ranges.push_back( { op->local(), op_index, op->live_range } );
        }
    }

    /*
        Sort live ranges.
    */
    std::sort
    (
        _local_ranges.begin(),
        _local_ranges.end(),
        []( const live_range& a, const live_range& b )
        {
            if ( a.local_index < b.local_index )
                return true;
            if ( a.local_index == b.local_index && a.lower < b.lower )
                return true;
            return false;
        }
    );

    /*
        Merge adjacent ranges.
    */
    unsigned next = _local_ranges.size() ? 1 : 0;
    for ( unsigned live_index = 1; live_index < _local_ranges.size(); ++live_index )
    {
        live_range* pr = &_local_ranges[ next - 1 ];
        live_range* lr = &_local_ranges[ live_index ];

        if ( lr->lower >= lr->upper )
        {
            continue;
        }

        if ( pr->local_index == lr->local_index && pr->upper == lr->lower )
        {
            pr->upper = lr->upper;
            continue;
        }

        _local_ranges[ next ] = *lr;
        next += 1;
    }
    _local_ranges.resize( next );

    /*
        Build index.
    */
    _local_values.resize( _f->ast->locals.size() );
    for ( unsigned live_index = 0; live_index < _local_ranges.size(); )
    {
        unsigned local_index = _local_ranges[ live_index ].local_index;

        live_local* value = &_local_values[ local_index ];
        value->live_index = live_index;
        value->live_count = 0;
        value->live_range = IR_INVALID_REGISTER;
        value->r = IR_INVALID_REGISTER;
        value->mark = false;

        while ( live_index < _local_ranges.size() && _local_ranges[ live_index ].local_index == local_index )
        {
            value->live_count += 1;
            value->live_range = _local_ranges[ live_index ].upper;
            ++live_index;
        }
    }
}

void ir_alloc::mark_pinning()
{
    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        ir_op* op = &_f->ops[ op_index ];

        op->mark = false;
        op->s = IR_INVALID_REGISTER;
        op->r = IR_INVALID_REGISTER;

        if ( op->live_range == IR_INVALID_INDEX )
        {
            continue;
        }

        if ( is_stacked( op ) )
        {
            /*
                Scan block for all ops which are live across this op (i.e. live
                at the next op).  We only need to check this block, because ops
                that survive blocks will have a REF/PHI in the header giving
                their live range in this block.
            */

            unsigned stacked_index = _stacked.size();
            stacked instruction = { op_index, 0 };

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
                    _stacked_across.emplace( check_index, stacked_index );
                    instruction.across_count += 1;
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
                    _stacked_across.emplace( check_index, stacked_index );
                    instruction.across_count += 1;
                }

                phi_index = phi->phi_next;
            }

            _stacked.push_back( instruction );
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

                if ( j == 0 && ( op->opcode == IR_EXTEND || op->opcode == IR_B_DEF ) )
                    continue;

                ir_op* pinned_op = &_f->ops.at( operand.index );
                if ( pinned_op->local() == IR_INVALID_LOCAL )
                {
                    if ( pinned_op->live_range == op_index )
                    {
                        pinned_op->mark = true;
                    }
                }
                else
                {
                    live_local* value = &_local_values.at( pinned_op->local() );
                    if ( value->live_range == op_index )
                    {
                        value->mark = true;
                    }
                }
            }
        }
    }
}

void ir_alloc::allocate()
{
    unsigned sweep_index = 0;

    // Anchor all stacked instructions which have no values live across them.
    for ( unsigned stacked_index = 0; stacked_index < _stacked.size(); ++stacked_index )
    {
        stacked* instruction = &_stacked.at( stacked_index );
        if ( instruction->across_count == 0 )
        {
            anchor_stacked( stacked_index, sweep_index );
        }
    }

    // Allocate result registers in program order.
    while ( ! _unpinned.empty() && sweep_index < _f->ops.size() )
    {
        while ( ! _unpinned.empty() )
        {
            unsigned op_index = _unpinned.top();
            _unpinned.pop();
            allocate_result( op_index, sweep_index );
        }

        allocate_result( sweep_index, sweep_index );
        sweep_index += 1;
    }
}

void ir_alloc::allocate_result( unsigned op_index, unsigned sweep_index )
{
    ir_op* op = &_f->ops.at( op_index );

    if ( op->opcode == IR_REF || op->opcode == IR_PHI )
    {
        return;
    }

    if ( op->local() != IR_INVALID_LOCAL )
    {


    }
    else
    {


    }
}

void ir_alloc::anchor_stacked( unsigned stacked_index, unsigned sweep_index )
{
    stacked* instruction = &_stacked.at( stacked_index );
    assert( instruction->across_count == 0 );
    ir_op* op = &_f->ops.at( instruction->index );

    // Determine stack top register.
    assert( op->s == IR_INVALID_REGISTER );
    op->s = _live_r->stack_top( instruction->index );

    // Unpin all values pinned by this op.
    unpin_operands( instruction->index, sweep_index );
}

void ir_alloc::unpin_operands( unsigned op_index, unsigned sweep_index )
{
    ir_op* op = &_f->ops.at( op_index );

    for ( unsigned j = 0; j < op->ocount; ++j )
    {
        ir_operand operand = _f->operands.at( op->oindex + j );

        if ( operand.kind != IR_O_OP )
            continue;

        unsigned def_index = IR_INVALID_INDEX;
        ir_op* pinned_op = &_f->ops.at( operand.index );
        if ( pinned_op->local() == IR_INVALID_LOCAL )
        {
            if ( pinned_op->mark && pinned_op->live_range == op_index )
            {
                pinned_op->mark = false;
                def_index = operand.index;
            }
            else
            {
                continue;
            }
        }
        else
        {
            live_local* value = &_local_values.at( pinned_op->local() );
            if ( value->mark && value->live_range == op_index )
            {
                value->mark = false;
                def_index = _local_ranges.at( value->live_index ).lower;
                assert( _f->ops.at( def_index ).local() == pinned_op->local() );
            }
            else
            {
                continue;
            }
        }

        assert( def_index != IR_INVALID_INDEX );
        if ( def_index <= sweep_index )
        {
            _unpinned.push( def_index );
        }
    }
}

bool ir_alloc::is_stacked( const ir_op* op )
{
    switch ( op->opcode )
    {
    case IR_CALL:
    case IR_YCALL:
    case IR_YIELD:
    case IR_VARARG:
    case IR_UNPACK:
    case IR_JUMP_RETURN:
    case IR_FOR_EACH_ITEMS:
        if ( op->unpack() > 1 )
        {
            return true;
        }
        if ( op->ocount > 1 )
        {
            return true;
        }
        if ( op->ocount == 1 )
        {
            ir_operand operand = _f->operands.at( op->oindex );
            if ( operand.kind == IR_O_OP && _f->ops.at( operand.index ).unpack() > 1 )
            {
                return true;
            }
        }
        return false;

    case IR_EXTEND:
    case IR_JUMP_FOR_SGEN:
        return true;

    default:
        return false;
    }
}

bool ir_alloc::is_pinning( const ir_op* op )
{
    switch ( op->opcode )
    {
    case IR_MOV:
    case IR_B_DEF:
    case IR_B_PHI:
        return true;

    default:
        return is_stacked( op ) && op->ocount > 1;
    }
}

void ir_alloc::debug_print()
{
    for ( unsigned i = 0; i < _local_values.size(); ++i )
    {
        const live_local* local_value = &_local_values[ i ];
        if ( ! local_value->live_count )
        {
            continue;
        }

        std::string_view name = _f->ast->locals.at( i ).name;
        printf( "VALUE ↓%04X",  local_value->live_range );

        if ( local_value->mark )
            printf( " !" );
        else if ( local_value->r != IR_INVALID_REGISTER )
            printf( " r" );
        else
            printf( "  " );

        if ( local_value->r != IR_INVALID_REGISTER )
            printf( "%02u", local_value->r );
        else
            printf( "  " );

        printf( " %u %.*s\n", i, (int)name.size(), name.data() );

        for ( unsigned j = 0; j < local_value->live_count; ++j )
        {
            const live_range* local_range = &_local_ranges[ local_value->live_index + j ];
            printf( "  :%04X ↓%04X\n", local_range->lower, local_range->upper );
        }
    }

}

}

