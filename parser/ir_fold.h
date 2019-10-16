//
//  ir_fold.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef IR_FOLD_H
#define IR_FOLD_H

/*
    The folding process performs the following transformations:

      - Phi operands which merge the same definition are simplified.
      - Expressions involving only constants are precomputed.
      - Conditional branches based on constant values are made unconditional.
      - Branch phi sequences based on constants are simplified.
      - Uses of single values are updated to use the target value.
      - Unreachable blocks are removed.
*/

#include "ir.h"

namespace kf
{

class ir_fold
{
public:

    explicit ir_fold( source* source );
    ~ir_fold();

    void fold( ir_function* function );

private:

    void fold_phi();
    void fold_phi_loop();
    bool phi_loop_search( ir_operand loop_phi, ir_operand operand );
    void fold_phi_step();

    void fold_constants();
    void fold_constants( ir_block* block );

    ir_operand jump_block_operand( unsigned operand_index );
    ir_operand fold_operand( unsigned operand_index );

    bool is_constant( ir_operand operand );
    bool is_upval( const ir_op* op );
    double to_number( ir_operand operand );
    std::string_view to_string( ir_operand operand );

    bool test_constant( ir_operand operand );
    std::pair< ir_operand, size_t > count_nots( ir_operand operand );

    bool fold_unarithmetic( ir_op* op );
    bool fold_biarithmetic( ir_op* op );
    bool fold_concat( ir_op* op );
    bool fold_equal( ir_op* op );
    bool fold_compare( ir_op* op );
    bool fold_not( ir_op* op );
    bool fold_cut( unsigned op_index, ir_op* op );
    bool fold_phi( ir_op* op );
    bool fold_test( ir_op* op );

    void fold_uses();

    void remove_unreachable_blocks();

    source* _source;
    ir_function* _f;
    std::vector< ir_operand > _stack;

};

ir_operand ir_fold_operand( ir_function* f, ir_operand operand );

}

#endif

