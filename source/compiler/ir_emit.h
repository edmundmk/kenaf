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
#include "code_unit.h"

namespace kf
{

class ir_emit
{
public:

    ir_emit( source* source, code_unit* unit );
    ~ir_emit();

    void emit( ir_function* function );

private:

    void emit_constants();

    void assemble();
    void op_unary( const ir_op* rop, opcode o );
    void op_binary( const ir_op* rop, opcode o );
    void op_addmul( const ir_op* rop, opcode o, opcode ok, opcode oi );
    void op_concat( const ir_op* rop );
    void op_const( const ir_op* rop );
    void emit( srcloc sloc, op op );

    source* _source;
    code_unit* _unit;
    ir_function* _f;
    std::unique_ptr< code_function_unit > _u;
    std::vector< op > _i;
    unsigned _max_r;

};

}

#endif

