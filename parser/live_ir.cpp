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

live_ir::live_ir( source* source )
    :   _source( source )
{
    (void)_source;
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

        During liveness analysis, the r field is used as a flag to indicate
        that the op has been made live, but its uses have not yet been marked.
    */
    _f = function;
    live_blocks();
}

void live_ir::reset( ir_function* function )
{
    for ( unsigned op_index = 0; op_index < function->ops.size(); ++op_index )
    {
        ir_op* op = &function->ops[ op_index ];
        op->mark = 0;
        op->live_range = IR_INVALID_INDEX;
    }
}

void live_ir::live_blocks()
{
    // Set work flags on all blocks, to prevent them being pushed on the
    // work stack until they've been processed once.
    for ( ir_block& block : _f->blocks )
    {
        block.mark = LIVE_BODY | LIVE_HEAD;
    }

    // Do an initial reverse pass through the block list, marking live ops.
    // This should make all values live except those referenced by loop edges.
    unsigned block_index = _f->blocks.size();
    while ( block_index-- )
    {
        ir_block* block = &_f->blocks[ block_index ];
        if ( block->kind == IR_BLOCK_NONE )
            continue;

        block->mark = 0;
        live_body( block_index, block );
        live_head( block_index, block );
    }

    // If ops are made live by loop edges, we need to mark values live
    // recursively.  Continue to process until there is no more work to do.
    while ( ! _work_stack.empty() )
    {
        ir_block_index block_index = _work_stack.back();
        _work_stack.pop_back();

        ir_block* block = &_f->blocks.at( block_index );
        unsigned block_mark = block->mark;
        block->mark = 0;

        if ( block_mark & LIVE_BODY )
        {
            // Ops in the body can make ops in the head live.
            live_body( block_index, block );
            live_head( block_index, block );
        }
        else if ( block_mark & LIVE_HEAD )
        {
            // Locals are live across the block but are not defined in it.
            live_head( block_index, block );
        }
    }
}

void live_ir::live_body( ir_block_index block_index, ir_block* block )
{
    /*
        References from successor blocks should have made some of our ops live.
        Visit each op, and if the r flag is set, mark its uses, potentially
        setting the r flag of other values in the block.  Also, some ops need
        to be live no matter what (e.g. return, call).
    */

    unsigned op_index = block->upper;
    while ( op_index-- > block->lower )
    {
        ir_op* op = &_f->ops[ op_index ];

        switch ( op->opcode )
        {
        case IR_PHI:
        case IR_REF:
        case IR_BLOCK:
            continue;

        case IR_JUMP:
        case IR_JUMP_FOR_EGEN:
        case IR_JUMP_FOR_SGEN:
        case IR_JUMP_TEST:
        case IR_JUMP_FOR_EACH:
        case IR_JUMP_FOR_STEP:
        case IR_JUMP_THROW:
        case IR_JUMP_RETURN:
        case IR_SET_UPVAL:
        case IR_SET_KEY:
        case IR_SET_INDEX:
        case IR_APPEND:
        case IR_CALL:
        case IR_YCALL:
        case IR_YIELD:
        case IR_EXTEND:
        case IR_CLOSE_UPSTACK:
            // These instructions have side effects so they need to
            // stay live no matter what.
            if ( ! op->mark )
            {
                op->mark = IR_MARK_STICKY;
                op->r = true;
            }
            break;

        default:
            break;
        }

        // Skip ops which are not live or which have already had uses marked.
        if ( ! op->r )
        {
            continue;
        }

        // Mark all ops used by this op.
        for ( unsigned j = 0; j < op->ocount; ++j )
        {
            ir_operand operand = _f->operands.at( op->oindex + j );
            if ( operand.kind != IR_O_OP )
            {
                continue;
            }

            mark_use( operand, op_index );
        }

        // Marked all uses.
        op->r = false;
    }
}

void live_ir::live_head( ir_block_index block_index, ir_block* block )
{
    /*
        Go through all ref/phi ops in the head of a block.  These reference
        ops in predecessor blocks, which potentially need to be processed.
    */

    // Get list of preceding blocks.
    ir_block_index* prblock_indexes = nullptr;
    unsigned prcount = 0;
    if ( block->preceding_lower < block->preceding_upper )
    {
        prblock_indexes = &_f->preceding_blocks.at( block->preceding_lower );
        prcount = block->preceding_upper - block->preceding_lower;
    }

    // Visit each op in the header.
    unsigned phi_index = block->phi_head;
    while ( phi_index != IR_INVALID_INDEX )
    {
        ir_op* phi = &_f->ops.at( phi_index );

        // Skip ops which are not live or which have already had uses marked.
        if ( ! phi->r )
        {
            phi_index = phi->phi_next;
            continue;
        }

        // Mark all defs in preceding blocks.
        for ( unsigned pr = 0; pr < prcount; ++pr )
        {
            ir_block_index prblock_index = prblock_indexes[ pr ];
            ir_block* prblock = &_f->blocks.at( prblock_index );

            // Find def incoming from this preceding block.
            ir_operand def;
            if ( phi->opcode == IR_REF )
            {
                assert( phi->ocount == 1 );
                def = _f->operands.at( phi->oindex );
            }
            else
            {
                assert( phi->ocount == prcount );
                def = _f->operands.at( phi->oindex + pr );
            }

            assert( def.kind == IR_O_OP );
            ir_op* op = &_f->ops.at( def.index );
            unsigned block_mark = 0;

            if ( op->opcode != IR_PHI && op->opcode != IR_REF &&
                def.index >= prblock->lower && def.index < prblock->upper )
            {
                // Def is in previous block's body.  Mark it directly.
                block_mark |= LIVE_BODY;
            }
            else
            {
                // Def was imported into the previous block's header.  There
                // must be a matching phi/ref in that header.
                def = match_phi( prblock, phi->local() );
                assert( def.kind == IR_O_OP );
                block_mark |= LIVE_HEAD;
            }

            if ( mark_use( def, prblock->upper ) )
            {
                // Op in predecessor block was made live.  Ensure we revisit.
                assert( block_mark );
                if ( ! prblock->mark )
                {
                    _work_stack.push_back( prblock_index );
                }
                prblock->mark |= block_mark;
            }
        }

        // Marked all uses.
        phi->r = false;
    }
}

ir_operand live_ir::match_phi( ir_block* block, unsigned local )
{
    // Search block header for a phi matching the local.
    unsigned phi_index = block->phi_head;
    while ( phi_index != IR_INVALID_INDEX )
    {
        ir_op* phi = &_f->ops.at( phi_index );

        if ( phi->local() == local )
        {
            return { IR_O_OP, phi_index };
        }

        phi_index = phi->phi_next;
    }

    // Should never happen!
    return { IR_O_NONE };
}

bool live_ir::mark_use( ir_operand def, unsigned use_index )
{
    assert( def.kind == IR_O_OP );
    ir_op* op = &_f->ops.at( def.index );

    // Increment mark.
    uint8_t old_mark = op->mark;
    uint8_t new_mark = op->mark + 1;
    op->mark = new_mark >= old_mark ? new_mark : IR_MARK_STICKY;

    // Update live_range.
    unsigned live_range = op->live_range != IR_INVALID_INDEX ? op->live_range : 0;
    op->live_range = std::max( live_range, use_index );

    // Set r flag if marking it made this op live.
    if ( old_mark == 0 )
    {
        op->r = true;
        return true;
    }
    else
    {
        return false;
    }
}

}

