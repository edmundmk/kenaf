//
//  ir_live.h
//
//  Created by Edmund Kapusniak on 14/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef IR_LIVE_H
#define IR_LIVE_H

/*
    Perform liveness analysis.  After this process, each op in the IR has
    'mark' set to the number of uses (saturating to 255), and live_range set
    to the index of the op in the block where the op dies (or the closing
    jump, if it survives the block).

    Liveness information for variables consists of a list of ops which define
    the variable.  The live ranges of these ops should not overlap.  Variables
    are also constructed for the hidden variables used by for loops.
*/

#include "ir.h"

namespace kf
{

class ir_live
{
public:

    explicit ir_live( source* source );
    ~ir_live();

    void live( ir_function* function );

private:

    enum { LIVE_BODY = 1 << 0, LIVE_HEAD = 1 << 1 };

    void reset();

    void live_blocks();
    void live_body( ir_block_index block_index, ir_block* block );
    void live_head( ir_block_index block_index, ir_block* block );
    ir_operand match_phi( ir_block* block, unsigned local_index );
    bool mark_use( ir_operand def, unsigned use_index );

    void erase_dead();

    source* _source;
    ir_function* _f;
    std::vector< ir_block_index > _work_stack;

};

}

#endif

