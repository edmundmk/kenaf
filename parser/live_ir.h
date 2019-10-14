//
//  live_ir.h
//
//  Created by Edmund Kapusniak on 14/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef LIVE_IR_H
#define LIVE_IR_H

/*
    Perform liveness analysis.  After this process, each op in the IR has
    'mark' set to the number of uses (saturating to 255), and live_range set
    to the index of the op in the block where the op dies (or the closing
    jump, if it survives the block).

    Liveness information for variables consists of a list of ops which define
    the variable.  The live ranges of these ops should not overlap.
*/

#include <unordered_set>
#include "ir.h"

namespace kf
{

class live_ir
{
public:

    explicit live_ir( source* source );
    ~live_ir();

    void live( ir_function* function );

private:

    void live_linear();
    void live_successor( ir_block_index block_index, ir_operand jump );
    void mark_use( ir_operand def, unsigned use_index );

    source* _source;
    ir_function* _f;

};

}

#endif

