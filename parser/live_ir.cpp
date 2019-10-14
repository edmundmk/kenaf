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
        which means that blocks are in dominance order already.  Liveness
        analysis proceeds like this:

          - A linear scan of all instructions from bottom to top, counting
            the number of uses for each value and the address of the last use
            in the block.

          - Only phi ops in loop headers are able to reference ops later in
            the dominance order.  If such a reference makes a value live when
            it wasn't before, the op is pushed onto a stack.

          - In the second stage, dataflow analysis is performed on ops in the
            stack to mark uses from backlinks in loop headers.

        Live ranges for variables are pushed when each op that defines the
        variable is made live.
    */

//    live_linear_pass();
//    live_dataflow_pass();
}

void live_ir::live_linear_pass()
{
    /*
        Scan entire instruction list from bottom to top, marking uses of each
        operand of each live op.
    */
    unsigned op_index = _f->ops.size();
    while ( op_index-- )
    {
        ir_op* op = &_f->ops[ op_index ];

        // Skip phi ops, they will be dealt with when we reach the block.
        if ( op->opcode == IR_PHI || op->opcode == IR_REF )
        {
            continue;
        }

        // Deal with block header.
        if ( op->opcode == IR_BLOCK )
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

