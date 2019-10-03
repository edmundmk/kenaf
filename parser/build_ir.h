//
//  build_ir.h
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef BUILD_IR_H
#define BUILD_IR_H

/*
    Traverses an AST and builds IR.
*/

#include <unordered_map>
#include "ir.h"
#include "ast.h"

namespace kf
{

class build_ir
{
public:

    explicit build_ir( source* source );
    ~build_ir();

    std::unique_ptr< ir_function > build( ast_function* function );

private:

    struct node_index
    {
        inline ast_node* operator -> () const { return node; }
        ast_node* node;
        unsigned index;
    };

    struct jump_fixup
    {
        unsigned oindex;
    };

    struct test_fixup
    {
        jump_fixup if_true;
        jump_fixup if_false;
    };

    // AST traversal.
    node_index child_node( node_index node );
    node_index next_node( node_index node );

    // Op helpers.
    ir_operand list_op( ir_opcode opcode, node_index node );
    template < typename F > ir_operand fold_unary( ir_opcode opcode, node_index node, F fold );
    template < typename F > ir_operand fold_binary( ir_opcode opcode, node_index node, F fold );
    ir_operand k_number( double n );

    // Visiting AST.
    ir_operand visit( node_index node );
    void visit_children( node_index node );

    // Emit ops.
    ir_operand emit( srcloc sloc, ir_opcode opcode, ir_operand* o, unsigned ocount );
    void close_upstack( node_index node );

    // Control flow.
    unsigned block_head( srcloc sloc );
    test_fixup block_test( srcloc sloc, ir_operand test );
    jump_fixup block_jump( srcloc sloc );
    void block_last( ir_operand last_op );
    void fixup( jump_fixup fixup, unsigned target );
    void break_continue( size_t break_index, unsigned loop_break, size_t continue_index, unsigned loop_continue );

    // Function under construction.
    source* _source;
    std::unique_ptr< ir_function > _f;

    // Operand stack.
    std::vector< ir_operand > _o;

    // Jump operand fixups.
    std::vector< jump_fixup > _jump_endif;
    std::vector< jump_fixup > _jump_break;
    std::vector< jump_fixup > _jump_continue;

};

}

#endif
