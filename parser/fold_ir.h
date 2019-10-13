//
//  fold_ir.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef FOLD_IR_H
#define FOLD_IR_H

/*
    The folding process performs the following transformations:

      - Expressions involving only constants are precomputed.
      - Conditional branches based on constant values are made unconditional.

      - Unreachable blocks are removed.
      - Instructions are marked if their value is used.
      - Unused instructions are removed.

      - Constants are inlined into instructions that allow constant operands.
      - Constants and selectors are allocated space in the function's tables.

*/

#include "ir.h"

namespace kf
{

class fold_ir
{
public:

    explicit fold_ir( source* source );
    ~fold_ir();

    void fold( ir_function* function );

private:

    void fold_constants();
    void fold_constants( ir_block* block );

    ir_block_index jump_block_index( unsigned operand_index );

    source* _source;
    ir_function* _f;
    std::vector< ir_block_index > _block_stack;

};

}

#endif

