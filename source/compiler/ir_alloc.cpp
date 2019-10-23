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
      - VARARG generates a value list.
      - Array UNPACK and EXTEND.
      - JUMP_RETURN consumes a value list.
      - JUMP_FOR_SGEN and JUMP_FOR_EGEN, as hidden variables are adjacent.
      - FOR_EACH_ITEMS generates a value list.

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
        and JUMP_FOR_SGEN.
      - An instruction which passes through its operand unchanged, i.e. MOV,
        B_DEF, or B_PHI.

    As a special case, the hidden loop variable is pinned to JUMP_FOR_SGEN or
    JUMP_FOR_EGEN.

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

ir_alloc::ir_alloc( source* source )
    :   _source( source )
    ,   _f( nullptr )
{
    (void)_source;
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
    assign_locals();

    _local_values.clear();
    _local_ranges.clear();
    _local_defs.clear();
    _stacked.clear();
    _stacked_across.clear();
    assert( _unpinned.empty() );
    _regmap.clear();
}

void ir_alloc::build_values()
{
    /*
        Build live ranges for each local by collecting each definition of it.
    */
    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        const ir_op* op = &_f->ops[ op_index ];

        if ( op->opcode == IR_BLOCK )
        {
            const ir_block* block = &_f->blocks[ _f->operands[ op->oindex ].index ];
            for ( unsigned phi_index = block->phi_head; phi_index != IR_INVALID_INDEX; )
            {
                const ir_op* phi = &_f->ops[ phi_index ];
                if ( phi->local() != IR_INVALID_LOCAL && phi->live_range != IR_INVALID_INDEX )
                {
                    _local_ranges.push_back( { phi->local(), op_index, phi->live_range } );
                }
                phi_index = phi->phi_next;
            }

            continue;
        }

        if ( op->opcode == IR_REF || op->opcode == IR_PHI || op->local() == IR_INVALID_LOCAL )
        {
            continue;
        }

        if ( op->local() != IR_INVALID_LOCAL && op->live_range != IR_INVALID_INDEX )
        {
            _local_ranges.push_back( { op->local(), op_index, op->live_range } );
            _local_defs.push_back( op_index );
        }
    }

    /*
        Sort live ranges.
    */
    std::sort
    (
        _local_ranges.begin(),
        _local_ranges.end(),
        []( const ir_value_range& a, const ir_value_range& b )
        {
            if ( a.local_index < b.local_index )
                return true;
            if ( a.local_index == b.local_index && a.lower < b.lower )
                return true;
            return false;
        }
    );

    std::sort
    (
        _local_defs.begin(),
        _local_defs.end(),
        [this]( unsigned a, unsigned b )
        {
            return _f->ops[ a ].local() < _f->ops[ b ].local();
        }
    );

    /*
         Merge adjacent ranges.
    */
    unsigned next = _local_ranges.size() ? 1 : 0;
    for ( unsigned live_index = 1; live_index < _local_ranges.size(); ++live_index )
    {
        ir_value_range* pr = &_local_ranges[ next - 1 ];
        ir_value_range* lr = &_local_ranges[ live_index ];

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

        local_value* value = &_local_values[ local_index ];
        value->op_index = _local_ranges[ live_index ].lower;
        value->live_range = IR_INVALID_REGISTER;
        value->live_index = live_index;
        value->live_count = 0;
        value->r = IR_INVALID_REGISTER;
        value->mark = false;

        while ( live_index < _local_ranges.size() && _local_ranges[ live_index ].local_index == local_index )
        {
            value->live_count += 1;
            value->live_range = _local_ranges[ live_index ].upper;
            ++live_index;
        }
    }
    for ( unsigned defs_index = 0; defs_index < _local_defs.size(); )
    {
        unsigned local_index = _f->ops[ _local_defs[ defs_index ] ].local();

        local_value* value = &_local_values[ local_index ];
        value->defs_index = defs_index;
        value->defs_count = 0;

        while ( defs_index < _local_defs.size() && _f->ops[ _local_defs[ defs_index ] ].local() == local_index )
        {
            value->defs_count += 1;
            ++defs_index;
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
                    unsigned op_index = check_op->local() == IR_INVALID_LOCAL ? check_index : _local_values.at( check_op->local() ).op_index;
                    _stacked_across.emplace( op_index, stacked_index );
                    instruction.across_count += 1;
                }
            }

            const ir_op* block_op = &_f->ops[ check_index ];
            assert( block_op->opcode == IR_BLOCK );
            const ir_block* block = &_f->blocks[ _f->operands[ block_op->oindex ].index ];

            unsigned phi_index = block->phi_head;
            while ( phi_index != IR_INVALID_INDEX )
            {
                ir_op* phi = &_f->ops[ phi_index ];

                if ( phi->live_range != IR_INVALID_INDEX && phi->live_range > op_index )
                {
                    unsigned op_index = _local_values.at( phi->local() ).op_index;
                    _stacked_across.emplace( op_index, stacked_index );
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
                ir_operand operand = _f->operands[ op->oindex + j ];

                if ( operand.kind != IR_O_OP )
                    continue;

                ir_op* pinned_op = &_f->ops[ operand.index ];
                if ( pinned_op->local() == IR_INVALID_LOCAL )
                {
                    if ( pinned_op->live_range == op_index )
                    {
                        pinned_op->mark = true;
                    }
                }
                else
                {
                    local_value* value = &_local_values.at( pinned_op->local() );
                    if ( value->live_range == op_index )
                    {
                        value->mark = true;
                    }
                }
            }
        }

        if ( op->opcode == IR_JUMP_FOR_SGEN || op->opcode == IR_JUMP_FOR_EGEN )
        {
            /*
                JUMP_FOR_SGEN/JUMP_FOR_EGEN local is pinned to the def.
            */
            assert( op->local() != IR_INVALID_LOCAL );
            local_value* value = &_local_values.at( op->local() );
            assert( value->op_index == op_index );
            value->mark = true;
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
            anchor_stacked( instruction );
        }
    }

    // Allocate result registers in program order.
    while ( ! _unpinned.empty() || sweep_index < _f->ops.size() )
    {
        if ( _unpinned.empty() || _unpinned.top().op_index > sweep_index )
        {
            allocate( sweep_index, IR_INVALID_REGISTER );
            sweep_index += 1;
        }
        else
        {
            unpinned_value unpinned = _unpinned.top();
            _unpinned.pop();
            allocate( unpinned.op_index, unpinned.prefer );
            if ( unpinned.op_index == sweep_index )
            {
                sweep_index += 1;
            }
        }
    }
}

void ir_alloc::allocate( unsigned op_index, unsigned prefer )
{
    ir_op* op = &_f->ops[ op_index ];
    if ( op->opcode == IR_REF || op->opcode == IR_PHI || op->opcode == IR_NOP )
    {
        return;
    }

    if ( op->local() == IR_INVALID_LOCAL )
    {
        if ( op->mark )
        {
            return;
        }

        assert( op->r == IR_INVALID_REGISTER );
        if ( has_result( op ) )
        {
            ir_value_range value_range = { IR_INVALID_LOCAL, op_index, op->live_range };
            op->r = allocate_register( op_index, prefer, &value_range, 1 );
        }
        across_stacked( op_index );
        unpin_move( op, op_index );
    }
    else
    {
        local_value* value = &_local_values.at( op->local() );
        if ( value->mark || value->op_index != op_index )
        {
            return;
        }

        assert( value->r == IR_INVALID_REGISTER );
        ir_value_range* ranges = &_local_ranges.at( value->live_index );
        value->r = allocate_register( value->op_index, prefer, ranges, value->live_count );
        across_stacked( op_index );

        for ( unsigned j = 0; j < value->defs_count; ++j )
        {
            unsigned def_index = _local_defs.at( value->defs_index + j );
            ir_op* def = &_f->ops[ def_index ];
            assert( def->local() == op->local() );
            def->r = value->r;
            unpin_move( def, def_index );
        }
    }
}

unsigned ir_alloc::allocate_register( unsigned op_index, unsigned prefer, ir_value_range* ranges, size_t rcount )
{
    assert( prefer <= IR_INVALID_REGISTER );
    ir_op* def = &_f->ops[ op_index ];

    // Special case for IR_JUMP_FOR_SGEN and IR_JUMP_FOR_EGEN hidden locals.
    if ( def->opcode == IR_JUMP_FOR_SGEN || def->opcode == IR_JUMP_FOR_EGEN )
    {
        assert( prefer == def->s );
        assert( def->s != IR_INVALID_LOCAL );

        unsigned r = def->s;
        unsigned hidden_count = def->opcode == IR_JUMP_FOR_SGEN ? 3 : 2;
        bool ok = false;
        while ( ! ok )
        {
            ok = true;
            for ( unsigned j = 0; j < hidden_count; ++j )
            {
                if ( ! _regmap.check( r + j, ranges, rcount ) )
                {
                    r = r + j + 1;
                    ok = false;
                    break;
                }
            }
        }

        for ( unsigned j = 0; j < hidden_count; ++j )
        {
            _regmap.check( r + j, ranges, rcount );
        }

        return r;
    }

    // Otherwise, pick register and allocate it.
    unsigned r = prefer;
    if ( def->opcode == IR_PARAM )
    {
        ir_operand operand = _f->operands[ def->oindex ];
        assert( operand.kind == IR_O_LOCAL );
        r = 1 + operand.index;
    }

    if ( r == IR_INVALID_REGISTER || ! _regmap.check( r, ranges, rcount ) )
    {
        r = _regmap.lowest( ranges, rcount );
    }

    _regmap.allocate( r, ranges, rcount );

    return r;
}

void ir_alloc::across_stacked( unsigned op_index )
{
    const auto irange = _stacked_across.equal_range( op_index );
    for ( auto i = irange.first; i != irange.second; ++i )
    {
        assert( i->first == op_index );

        stacked* instruction = &_stacked.at( i->second );
        assert( instruction->across_count );

        instruction->across_count -= 1;
        if ( ! instruction->across_count )
        {
            anchor_stacked( instruction );
        }
    }
}

void ir_alloc::anchor_stacked( stacked* instruction )
{
    assert( instruction->across_count == 0 );
    ir_op* op = &_f->ops[ instruction->index ];

    // Unpack operands have stack top associated with the op that uses them.
    if ( op->unpack() == IR_UNPACK_ALL )
    {
        return;
    }

    // Determine stack top register.
    assert( op->s == IR_INVALID_REGISTER );
    op->s = _regmap.top( instruction->index );
    unpin_stacked( op, instruction->index );

    // Recursively set stack top register for unpack arguments.
    while ( true )
    {
        if ( op->ocount < 1 )
            return;

        ir_operand operand = _f->operands[ op->oindex + op->ocount - 1 ];
        if ( operand.kind != IR_O_OP )
            return;

        ir_op* unpack = &_f->ops[ operand.index ];
        if ( unpack->unpack() != IR_UNPACK_ALL )
            return;

        if ( op->opcode != IR_EXTEND )
            unpack->s = op->s + op->ocount - 1;
        else
            unpack->s = op->s;
        unpin_stacked( unpack, operand.index );
        op = unpack;
    }
}

void ir_alloc::unpin_stacked( const ir_op* op, unsigned op_index )
{
    assert( op->s != IR_INVALID_REGISTER );
    unpin_operands( op, op_index, UNPIN_S );

    if ( op->opcode == IR_JUMP_FOR_SGEN || op->opcode == IR_JUMP_FOR_EGEN )
    {
        assert( op->local() != IR_INVALID_LOCAL );
        local_value* value = &_local_values.at( op->local() );
        assert( value->op_index == op_index );
        assert( value->mark );
        value->mark = false;
        _unpinned.push( { op_index, op->s } );
    }
}

void ir_alloc::unpin_move( const ir_op* op, unsigned op_index )
{
    if ( op->opcode == IR_MOV )
    {
        assert( op->r != IR_INVALID_REGISTER );
        unpin_operands( op, op_index, UNPIN_R );
    }
}

void ir_alloc::unpin_operands( const ir_op* op, unsigned op_index, unpin_rs rs )
{
    for ( unsigned j = 0; j < op->ocount; ++j )
    {
        ir_operand operand = _f->operands[ op->oindex + j ];

        if ( operand.kind != IR_O_OP )
            continue;

        unsigned def_index = IR_INVALID_INDEX;
        ir_op* pinned_op = &_f->ops[ operand.index ];
        if ( pinned_op->local() == IR_INVALID_LOCAL )
        {
            if ( pinned_op->mark && ( pinned_op->live_range == op_index ) )
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
            local_value* value = &_local_values.at( pinned_op->local() );
            if ( value->mark && ( value->live_range == op_index ) )
            {
                value->mark = false;
                def_index = value->op_index;
                assert( _f->ops[ def_index ].local() == pinned_op->local() );
            }
            else
            {
                continue;
            }
        }

        unsigned prefer;
        if ( rs == UNPIN_S )
            prefer = op->s + j;
        else
            prefer = op->r;

        assert( def_index != IR_INVALID_INDEX );
        _unpinned.push( { def_index, prefer } );
    }
}

bool ir_alloc::is_stacked( const ir_op* op )
{
    switch ( op->opcode )
    {
    case IR_CALL:
    case IR_VARARG:
    case IR_UNPACK:
    case IR_JUMP_RETURN:
    case IR_FOR_EACH_ITEMS:
        if ( op->unpack() > 1 )
            return true;
        if ( op->ocount > 1 )
            return true;
        if ( op->ocount == 1 )
        {
            ir_operand operand = _f->operands[ op->oindex ];
            if ( operand.kind == IR_O_OP && _f->ops[ operand.index ].unpack() > 1 )
                return true;
        }
        return false;

    case IR_YCALL:
    case IR_YIELD:
    case IR_EXTEND:
    case IR_JUMP_FOR_SGEN:
    case IR_JUMP_FOR_EGEN:
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
        return true;

    case IR_EXTEND:
        return false;

    default:
        return is_stacked( op ) && op->ocount > 1;
    }
}

bool ir_alloc::has_result( const ir_op* op )
{
    switch ( op->opcode )
    {
    case IR_EQ:
    case IR_NE:
    case IR_LT:
    case IR_LE:
    case IR_SET_KEY:
    case IR_SET_INDEX:
    case IR_SET_ENV:
    case IR_APPEND:
    case IR_EXTEND:
    case IR_BLOCK:
    case IR_JUMP:
    case IR_JUMP_TEST:
    case IR_JUMP_THROW:
    case IR_JUMP_RETURN:
    case IR_JUMP_FOR_EACH:
    case IR_JUMP_FOR_STEP:
        return false;

    case IR_CALL:
    case IR_YCALL:
    case IR_YIELD:
    case IR_VARARG:
    case IR_UNPACK:
        return op->unpack() == 1;

    default:
        return true;
    }
}

void ir_alloc::assign_locals()
{
    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        ir_op* op = &_f->ops[ op_index ];

        if ( op->local() == IR_INVALID_LOCAL )
            continue;

        const local_value* value = &_local_values.at( op->local() );
        assert( op->r == IR_INVALID_REGISTER || op->r == value->r );
        op->r = value->r;
    }
}

void ir_alloc::debug_print() const
{
    for ( unsigned i = 0; i < _local_values.size(); ++i )
    {
        const local_value* value = &_local_values[ i ];
        if ( ! value->live_count )
        {
            continue;
        }

        std::string_view name = _f->ast->locals[ i ].name;
        printf( "VALUE ↓%04X",  value->live_range );

        if ( value->mark )
            printf( " !" );
        else if ( value->r != IR_INVALID_REGISTER )
            printf( " r" );
        else
            printf( "  " );

        if ( value->r != IR_INVALID_REGISTER )
            printf( "%02u", value->r );
        else
            printf( "  " );

        printf( " %u %.*s\n", i, (int)name.size(), name.data() );

        for ( unsigned j = 0; j < value->live_count; ++j )
        {
            const ir_value_range* local_range = &_local_ranges[ value->live_index + j ];
            printf( "  :%04X ↓%04X\n", local_range->lower, local_range->upper );
        }
    }

}

}

