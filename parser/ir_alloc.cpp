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

