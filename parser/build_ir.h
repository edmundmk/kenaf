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

    build_ir();
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

    // Visit AST node, producing a value.
    ir_operand visit( node_index node );

    // Visit child nodes.
    void visit_children( node_index node );
    ir_operand visit_children_op( srcloc sloc, ir_opcode opcode, node_index node );

    // Emit ops.
    ir_operand op( srcloc sloc, ir_opcode opcode, ir_operand* o, unsigned ocount );
    void close_upstack( node_index node );

    // Control flow.
    unsigned block_head( srcloc sloc );
    test_fixup block_test( srcloc sloc, ir_operand test );
    jump_fixup block_jump( srcloc sloc );
    void block_last( ir_operand last_op );
    void fixup( jump_fixup fixup, unsigned target );
    void break_continue( size_t break_index, unsigned loop_break, size_t continue_index, unsigned loop_continue );

    // Function under construction.
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
