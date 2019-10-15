//
//  alloc_ir.h
//
//  Created by Edmund Kapusniak on 15/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef ALLOC_IR_H
#define ALLOC_IR_H

/*
    First, operand selection.  Certain bytecode instructions have forms which
    use constant or immediate operands, and we can avoid emitting load
    instructions by inlining constant values.

    Constants and selectors must be allocated indexes in the final tables.

    Finally, register allocation.  Registers are allocated to values in a
    fashion which attempts to both minimize unecessary moves and minimize the
    total number of registers used.
*/

#include "ir.h"

namespace kf
{

class alloc_ir
{
public:

    alloc_ir( source* source );
    ~alloc_ir();

    void alloc( ir_function* function );

private:

    source* _source;
    ir_function* _f;

};

}

#endif

