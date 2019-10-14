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
    live_linear_pass();
//    live_dataflow_pass();
}

void live_ir::live_linear_pass()
{
    /*
        Scan entire instruction list from bottom to top, marking uses of
        each operand of each live op.  This gives us a good first pass at
        liveness, though it ignores variables live across loop edges.
    */
    unsigned op_index = _f->ops.size();
    while ( op_index-- )
    {
        ir_op* op = &_f->ops[ op_index ];
        if ( op->opcode == IR_PHI || op->opcode == IR_REF )
        {
            continue;
        }

        // Deal with block header.
        if ( op->opcode == IR_BLOCK )
        {

        }

        // And with block footer.
        if ( op->opcode >= IR_JUMP && op->opcode <= IR_JUMP_RETURN )
        {
        }

        // Mark ops with side effects.
        if ( has_side_effects( op ) )
        {
            op->mark = IR_MARK_STICKY;
        }

        // Skip ops with no uses.
        if ( op->mark == 0 )
        {
            continue;
        }

        // Mark uses for op.

    }
}

bool live_ir::has_side_effects( ir_op* op )
{
    switch ( op->opcode )
    {
    case IR_SET_UPVAL:
    case IR_SET_KEY:
    case IR_SET_INDEX:
    case IR_APPEND:
    case IR_CALL:
    case IR_YCALL:
    case IR_YIELD:
    case IR_EXTEND:
    case IR_CLOSE_UPSTACK:
    case IR_JUMP_THROW:
    case IR_JUMP_RETURN:
        return true;

    default:
        return false;
    }
}



}

