//
//  alloc_k_ir.cpp
//
//  Created by Edmund Kapusniak on 15/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "alloc_k_ir.h"
#include "fold_ir.h"

namespace kf
{

alloc_k_ir::alloc_k_ir( source* source )
    :   _source( source )
{
    (void)_source;
}

alloc_k_ir::~alloc_k_ir()
{
}

void alloc_k_ir::alloc_k( ir_function* function )
{
    _f = function;
    alloc_operands();
}

void alloc_k_ir::alloc_operands()
{
    /*
        The following constant operands can be inlined:

            ADD v, c            ->  ADDK/ADDI v, c
            ADD c, v            ->  ADDK/ADDI v, c
            SUB v, c            ->  ADDK/ADDI v, -c
            SUB c, v            ->  SUBK/SUBI v, c
            MUL v, c            ->  MULK/MULI v, c
            MUL c, v            ->  MULK/MULI v, c
            CONCAT v, c         ->  CONCATK v, c
            CONCAT c, v         ->  RCONCATK v, c
            EQ v, c; JUMP       ->  JEQK v, c
            NE v, c; JUMP       ->  JNEK v, c
            LT v, c; JUMP       ->  JLTK v, c
            LT c, v; JUMP       ->  JGTK v, c
            LE v, c; JUMP       ->  JLEK v, c
            LE c, v; JUMP       ->  JGEK v, c
            GET_INDEX v, c      ->  GET_INDEXK/GET_INDEXI v, c
            SET_INDEX v, c, u   ->  SET_INDEXK/GET_INDEXK v, c, u

    */

    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        ir_op* op = &_f->ops[ op_index ];

        switch ( op->opcode )
        {
        case IR_ADD:
        case IR_MUL:
        {
            ir_operand* u = &_f->operands.at( op->oindex + 0 );
            ir_operand* v = &_f->operands.at( op->oindex + 1 );
            ir_operand fold_u = ir_fold_operand( _f, *u );
            ir_operand fold_v = ir_fold_operand( _f, *v );

            if ( fold_v.kind == IR_O_NUMBER )
            {
                // Second operand is constant.
                *v = operand_imm8( fold_v );
            }
            else if ( fold_u.kind == IR_O_NUMBER )
            {
                // Operation is commutative, switch operands.
                *u = *v;
                *v = operand_imm8( fold_u );
            }

            break;
        }

        case IR_SUB:
        {
            ir_operand* u = &_f->operands.at( op->oindex + 0 );
            ir_operand* v = &_f->operands.at( op->oindex + 1 );
            ir_operand fold_u = ir_fold_operand( _f, *u );
            ir_operand fold_v = ir_fold_operand( _f, *v );

            if ( fold_v.kind == IR_O_NUMBER )
            {
                // Convert to ADD with negated operand.
                double constant = _f->constants.at( fold_v.index ).n;
                fold_v.index = _f->constants.size();
                _f->constants.push_back( ir_constant( -constant ) );
                op->opcode = IR_ADD;
                *v = operand_imm8( fold_v );
            }
            else if ( fold_u.kind == IR_O_NUMBER )
            {
                // First operand is constant.
                *u = operand_imm8( fold_u );
            }

            break;
        }

        case IR_CONCAT:
        {

            break;
        }


        default : break;
        }
    }
}

ir_operand alloc_k_ir::operand_imm8( ir_operand operand )
{
    if ( operand.kind == IR_O_NUMBER )
    {
        double number = _f->constants.at( operand.index ).n;
        int8_t imm8 = (int8_t)number;
        if ( (double)imm8 == number )
        {
            operand = { IR_O_IMM8, (unsigned)imm8 };
        }
    }
    return operand;
}

}
