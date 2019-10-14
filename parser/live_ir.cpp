//
//  live_ir.cpp
//
//  Created by Edmund Kapusniak on 14/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "live_ir.h"

namespace kf
{

const uint8_t IR_MARK_STICKY = 0xFF;

inline uint8_t sticky_add( uint8_t a, uint8_t b )
{
    uint8_t c = a + b;
    return c >= a ? a : IR_MARK_STICKY;
}


live_ir::live_ir( source* source )
    :   _source( source )
{
}

live_ir::~live_ir()
{
}

void live_ir::live( ir_function* function )
{
    /*
        Our language has no goto, and the IR has been built in program order,
        which means that blocks are in dominance order already.  Additionally,
        uses in the body of a block must reference either another op in the
        block or a PHI/REF from the block header.
    */
    _f = function;
    live_linear();
}

void live_ir::live_linear()
{
    /*
        Scan entire instruction list from bottom to top, marking uses of
        each operand of each live op.  This gives us a good first pass at
        liveness, though it ignores variables live across loop edges.
    */
    unsigned block_index = _f->blocks.size();
    while ( block_index-- )
    {
        ir_block* block = &_f->blocks[ block_index ];

        unsigned op_index = block->upper;
        while ( op_index-- > block->lower )
        {

            ir_op* op = &_f->ops[ op_index ];

            switch ( op->opcode )
            {
            case IR_PHI:
            case IR_REF:
                continue;

            case IR_BLOCK:
                continue;

            case IR_JUMP:
            case IR_JUMP_FOR_EGEN:
            case IR_JUMP_FOR_SGEN:
                assert( op->ocount >= 1 );
                live_successor( block_index, _f->operands.at( op->oindex + op->ocount - 1 ) );
                op->mark = IR_MARK_STICKY;
                break;

            case IR_JUMP_TEST:
            case IR_JUMP_FOR_EACH:
            case IR_JUMP_FOR_STEP:
                assert( op->ocount >= 2 );
                live_successor( block_index, _f->operands.at( op->oindex + op->ocount - 2 ) );
                live_successor( block_index, _f->operands.at( op->oindex + op->ocount - 1 ) );
                op->mark = IR_MARK_STICKY;
                break;

            case IR_JUMP_THROW:
            case IR_JUMP_RETURN:
                op->mark = IR_MARK_STICKY;
                break;

            case IR_SET_UPVAL:
            case IR_SET_KEY:
            case IR_SET_INDEX:
            case IR_APPEND:
            case IR_CALL:
            case IR_YCALL:
            case IR_YIELD:
            case IR_EXTEND:
            case IR_CLOSE_UPSTACK:
                // These opcodes have side effects so they need to stay live.
                op->mark = IR_MARK_STICKY;
                break;

            default: break;
            }

            // Skip ops with no uses.
            if ( op->mark == 0 )
            {
                continue;
            }

            // Mark all ops used by this op.
            for ( unsigned j = 0; j < op->ocount; ++j )
            {
                ir_operand operand = _f->operands.at( op->oindex + j );
                if ( operand.kind == IR_O_OP )
                {
                    mark_use( operand, op_index );
                }
            }
        }
    }
}

void live_ir::live_successor( ir_block_index block_index, ir_operand jump )
{
    /*
        Examine variables live in the header of a successor block.  For each,
        either the definition is in this block, in which case mark the op, or
        we should have a PHI or REF in our header that matches it, in which
        case mark that op.
    */

    // Find this block.
    ir_block* this_block = &_f->blocks.at( block_index );

    // Find successor block.
    assert( jump.kind == IR_O_JUMP );
    ir_op* next_block_op = &_f->ops.at( jump.index );
    assert( next_block_op->opcode == IR_BLOCK );
    assert( next_block_op->ocount == 1 );
    ir_operand next_block_operand = _f->operands.at( next_block_op->oindex );
    assert( next_block_operand.kind == IR_O_BLOCK );
    ir_block* next_block = &_f->blocks.at( next_block_operand.index );

    // Find index of this block in preceding blocks list.
    unsigned prindex = 0;
    unsigned prcount = next_block->preceding_upper - next_block->preceding_lower;
    for ( ; prindex < prcount; ++prindex )
    {
        unsigned prblock_index = _f->preceding_blocks.at( next_block->preceding_lower + prindex );
        if ( prblock_index == block_index )
        {
            break;
        }
    }

    assert( prindex < prcount );

    // Go through phi/ref ops in successor block.
    unsigned phi_index = next_block->phi_head;
    while ( phi_index != IR_INVALID_INDEX )
    {
        ir_op* phi = &_f->ops.at( phi_index );

        // Skip unmarked phi ops.
        if ( phi->mark == 0 )
        {
            phi_index = phi->phi_next;
            continue;
        }

        // Find def referenced by op.
        ir_operand def;
        if ( phi->opcode == IR_REF )
        {
            def = _f->operands.at( phi->oindex );
        }
        else
        {
            assert( phi->ocount == prcount );
            def = _f->operands.at( phi->oindex + prindex );
        }

        assert( def.kind == IR_O_OP );
        ir_op* op = &_f->ops.at( def.index );

        if ( op->opcode != IR_PHI && op->opcode != IR_REF
            && def.index >= this_block->lower && def.index < this_block->upper )
        {
            // Def is in this block.
            mark_use( def, this_block->upper );
        }
        else
        {
            // Def is not in the block, should match in the header.
            unsigned local = op->local();
            assert( local != IR_INVALID_LOCAL );

            ir_op* this_phi = nullptr;
            unsigned index = this_block->phi_head;
            while ( index != IR_INVALID_INDEX )
            {
                this_phi = &_f->ops.at( index );
                if ( this_phi->local() == local )
                {
                    mark_use( { IR_O_OP, index }, this_block->upper );
                    break;
                }

                index = this_phi->phi_next;
            }

            assert( index != IR_INVALID_INDEX );
        }

        phi_index = phi->phi_next;
    }
}

void live_ir::mark_use( ir_operand def, unsigned use_index )
{
    assert( def.kind == IR_O_OP );
    ir_op* op = &_f->ops.at( def.index );

    // Increment mark.
    uint8_t mark = op->mark + 1;
    op->mark = mark >= op->mark ? mark : IR_MARK_STICKY;

    // Update live_range.
    unsigned live_range = op->live_range != IR_INVALID_INDEX ? op->live_range : 0;
    op->live_range = std::max( live_range, use_index );
}

}

