//
//  ir_live.cpp
//
//  Created by Edmund Kapusniak on 14/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ir_live.h"
#include "ir_fold.h"

namespace kf
{

ir_live::ir_live( report* report )
    :   _report( report )
    ,   _f( nullptr )
{
    (void)_report;
}

ir_live::~ir_live()
{
}

void ir_live::live( ir_function* function )
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
    reset();
    live_blocks();
    erase_dead();
}

void ir_live::reset()
{
    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        ir_op* op = &_f->ops[ op_index ];
        op->mark = false;
        op->s = 0;
        op->live_range = IR_INVALID_INDEX;
    }
}

void ir_live::live_blocks()
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

        ir_block* block = &_f->blocks[ block_index ];
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

void ir_live::live_body( ir_block_index block_index, ir_block* block )
{
    /*
        References from successor blocks should have made some of our ops live.
        Visit each op, and if the mark flag is set, mark its uses, potentially
        setting the mark flag of other values in the block.  Also, some ops need
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
            continue;

        case IR_BLOCK:
        case IR_JUMP:
        case IR_JUMP_TEST:
        case IR_JUMP_THROW:
        case IR_JUMP_RETURN:
        case IR_JUMP_FOR_EGEN:
        case IR_JUMP_FOR_EACH:
        case IR_JUMP_FOR_SGEN:
        case IR_JUMP_FOR_STEP:
        case IR_FOR_EACH_ITEMS:
        case IR_FOR_STEP_INDEX:
        case IR_SET_KEY:
        case IR_SET_INDEX:
        case IR_SET_ENV:
        case IR_APPEND:
        case IR_CALL:
        case IR_YCALL:
        case IR_YIELD:
        case IR_EXTEND:
            // These instructions have side effects so they need to
            // stay live no matter what.
            if ( ! op->s )
            {
                op->mark = true;
                op->s = IR_LIVE_STICKY;
                op->live_range = op_index;
            }
            break;

        default:
            break;
        }

        // Skip ops which are not live or which have already had uses marked.
        if ( ! op->mark )
        {
            continue;
        }

        // Mark all ops used by this op.
        for ( unsigned j = 0; j < op->ocount; ++j )
        {
            ir_operand operand = _f->operands[ op->oindex + j ];
            if ( operand.kind != IR_O_OP )
            {
                continue;
            }

            unsigned use_index = op_index;

            // First argument to EXTEND is live across instruction.
            if ( op->opcode == IR_EXTEND && j == 0 )
            {
                use_index += 1;
            }

            mark_use( operand, use_index );
        }

        // Marked all uses.
        op->mark = false;
    }
}

void ir_live::live_head( ir_block_index block_index, ir_block* block )
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
        prblock_indexes = &_f->preceding_blocks[ block->preceding_lower ];
        prcount = block->preceding_upper - block->preceding_lower;
    }

    // Visit each op in the header.
    unsigned phi_index = block->phi_head;
    while ( phi_index != IR_INVALID_INDEX )
    {
        ir_op* phi = &_f->ops[ phi_index ];

        // Skip ops which are not live or which have already had uses marked.
        if ( ! phi->mark )
        {
            phi_index = phi->phi_next;
            continue;
        }

        // Mark all defs in preceding blocks.
        for ( unsigned pr = 0; pr < prcount; ++pr )
        {
            ir_block_index prblock_index = prblock_indexes[ pr ];
            ir_block* prblock = &_f->blocks[ prblock_index ];

            // Find def incoming from this preceding block.
            ir_operand def;
            if ( phi->opcode == IR_REF )
            {
                assert( phi->ocount == 1 );
                def = _f->operands[ phi->oindex ];
            }
            else
            {
                assert( phi->ocount == prcount );
                def = _f->operands[ phi->oindex + pr ];
            }

            assert( def.kind == IR_O_OP );
            ir_op* op = &_f->ops[ def.index ];
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
        phi->mark = false;
    }
}

ir_operand ir_live::match_phi( ir_block* block, unsigned local_index )
{
    // Search block header for a phi matching the local.
    unsigned phi_index = block->phi_head;
    while ( phi_index != IR_INVALID_INDEX )
    {
        ir_op* phi = &_f->ops[ phi_index ];

        if ( phi->local() == local_index )
        {
            return { IR_O_OP, phi_index };
        }

        phi_index = phi->phi_next;
    }

    // Should never happen!
    return { IR_O_NONE };
}

bool ir_live::mark_use( ir_operand def, unsigned use_index )
{
    assert( def.kind == IR_O_OP );
    ir_op* op = &_f->ops[ def.index ];

    // Increment s.
    uint8_t old_s = op->s;
    uint8_t new_s = op->s + 1;
    op->s = new_s >= old_s ? new_s : IR_LIVE_STICKY;

    // Update live_range.
    unsigned live_range = op->live_range != IR_INVALID_INDEX ? op->live_range : 0;
    op->live_range = std::max( live_range, use_index );

    // Set mark flag if marking it made this op live.
    if ( old_s == 0 )
    {
        op->mark = true;
        return true;
    }
    else
    {
        return false;
    }
}

void ir_live::erase_dead()
{
    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        ir_op* op = &_f->ops[ op_index ];
        if ( op->live_range == IR_INVALID_INDEX )
        {
            op->opcode = IR_NOP;
            op->ocount = 0;
            op->oindex = IR_INVALID_INDEX;
            op->set_local( IR_INVALID_LOCAL );
        }
    }
}

}

