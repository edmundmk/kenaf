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

    struct op_branch
    {
        ir_operand op;
        jump_fixup branch;
    };

    // AST traversal.
    node_index child_node( node_index node );
    node_index next_node( node_index node );

    // Visiting AST.
    ir_operand visit( node_index node );
    void visit_children( node_index node );

    // Rvals and unpacking
    unsigned rval_list( node_index node, unsigned rvcount );
    ir_operand expr_unpack( node_index node, unsigned unpack );
    ir_operand expr_list_op( node_index node, ir_opcode opcode );

    // Constants.
    ir_operand number_operand( node_index node );
    ir_operand string_operand( node_index node );

    // Emit ops.
    ir_operand emit( srcloc sloc, ir_opcode opcode, unsigned ocount );
    op_branch emit_branch( srcloc sloc, ir_opcode opcode, unsigned ocount );
    void close_upstack( node_index node );

    // Control flow.
    unsigned block_head( srcloc sloc );
    jump_fixup block_jump( srcloc sloc );
    test_fixup block_test( srcloc sloc, ir_opcode opcode, ir_operand test );
    void block_last( srcloc sloc, ir_opcode opcode, unsigned ocount );

    // Fixing up jumps.
    void fixup( jump_fixup fixup, unsigned target );
    void fixup( std::vector< jump_fixup >* fixup_list, size_t index, unsigned target );

    // Use/def.
    void load( ir_operand operand );
    void def( srcloc sloc, unsigned local, ir_operand operand );

    // Function under construction.
    source* _source;
    std::unique_ptr< ir_function > _f;

    // Operand stack.
    std::vector< ir_operand > _o;

    // Jump operand fixups.
    std::vector< jump_fixup > _fixup_endif;
    std::vector< jump_fixup > _fixup_loopb;
    std::vector< jump_fixup > _fixup_loopc;

};

}

#endif
