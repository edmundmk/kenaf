//
//  ir_emit.h
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef IR_EMIT_H
#define IR_EMIT_H

/*
    Takes IR that's been through register allocation and emits bytecode.
*/

#include "ir.h"
#include "../common/code.h"

namespace kf
{

class ir_emit
{
public:

    ir_emit();
    ~ir_emit();

    void emit( ir_function* function );

private:

    std::vector< op > _i;

};

}

#endif
