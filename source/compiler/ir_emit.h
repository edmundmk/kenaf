//
//  ir_emit.h
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_IR_EMIT_H
#define KF_IR_EMIT_H

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

    ir_emit( report* report, code_unit* unit );
    ~ir_emit();

    void emit( ir_function* function );

private:

    enum emit_kind { AB, AB_SWAP, AB_NO_R, C, J, JUMP, J_SWAP };

    struct emit_shape
    {
        ir_opcode iopcode;
        unsigned ocount;
        ir_operand_kind okind[ 3 ];
        opcode copcode;
        emit_kind kind;
    };

    static const emit_shape SHAPES[];

    struct jump_fixup
    {
        unsigned jaddress;
        unsigned iaddress;
    };

    struct jump_label
    {
        unsigned iaddress;
        unsigned caddress;
    };

    struct move_entry
    {
        srcloc sloc;
        unsigned target;
        unsigned source;
    };

    void emit_constants();

    void assemble();
    unsigned with_shape( unsigned op_index, const ir_op* iop, const emit_shape* shape );
    unsigned with_moves( unsigned op_index, const ir_op* iop );
    unsigned with_stacked( unsigned op_index, const ir_op* iop );
    unsigned with_for_each( unsigned op_index, const ir_op* iop );
    unsigned with_for_step( unsigned op_index, const ir_op* iop );

    unsigned next( unsigned op_index, ir_opcode iopcode );
    bool match_operands( const ir_op* iop, const emit_shape* shape );

    const ir_op* u_operand( const ir_op* iop );
    bool check_r( const ir_op* iop, const char* detail );
    bool check_s( const ir_op* iop, const char* detail );

    void move( srcloc sloc, unsigned target, unsigned source );
    bool move_is_source( unsigned source );
    void move_emit();

    void emit( srcloc sloc, op op );

    void fixup_jumps();

    report* _report;
    code_unit* _unit;
    ir_function* _f;
    std::unique_ptr< code_function_unit > _u;
    std::vector< jump_fixup > _fixups;
    std::vector< jump_label > _labels;
    std::vector< move_entry > _moves;
    unsigned _max_r;

};

}

#endif

