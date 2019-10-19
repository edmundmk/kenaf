//
//  ir_emit.cpp
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ir_emit.h"

namespace kf
{

ir_emit::ir_emit( source* source )
    :   _source( source )
    ,   _f( nullptr )
{
}

ir_emit::~ir_emit()
{
}

void ir_emit::emit( ir_function* function )
{
    _f = function;
}

}

