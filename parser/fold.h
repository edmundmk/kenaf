//
//  fold.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef FOLD_H
#define FOLD_H

#include "ir.h"

namespace kf
{

/*
    Perform constant folding, make jumps unconditional, mark reachable
    blocks and instructions.
*/

void fold_constants( ir_function* function );

/*
    Fold operands into instructions that will have non-register forms.
    Allocate constants, prefer low-number constant slots in c operand.
*/

void fold_operands( ir_function* function );

}

#endif

