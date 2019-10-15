//
//  alloc_ir.cpp
//
//  Created by Edmund Kapusniak on 15/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "alloc_ir.h"

namespace kf
{

alloc_ir::alloc_ir( source* source )
    :   _source( source )
{
}

alloc_ir::~alloc_ir()
{
}

void alloc_ir::alloc( ir_function* function )
{
    _f = function;
}

}

