//
//  alloc_k_ir.h
//
//  Created by Edmund Kapusniak on 15/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef ALLOC_K_IR_H
#define ALLOC_K_IR_H

/*
    Builds final constant tables and inlines constant operands into
    instruction forms that support constant operands.
*/

#include "ir.h"

namespace kf
{

class alloc_k_ir
{
public:

    alloc_k_ir( source* source );
    ~alloc_k_ir();

    void alloc_k( ir_function* function );

private:

    void alloc_operands();
    ir_operand operand_imm8( ir_operand operand );

    source* _source;
    ir_function* _f;

};

}

#endif

