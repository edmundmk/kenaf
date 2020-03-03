//
//  ir_build.h
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_IR_BUILD_H
#define KF_IR_BUILD_H

/*
    Traverses an AST and builds IR.  Performs SSA form construction using
    ideas from this paper:
        http://compilers.cs.uni-saarland.de/papers/bbhlmz13cc.pdf
*/

#include <unordered_map>
#include <list>
#include "ir.h"
#include "ast.h"

namespace kf
{

class ir_build
{
public:

    explicit ir_build( report* report );
    ~ir_build();

    std::unique_ptr< ir_function > build( ast_function* function );

private:

    struct goto_fixup
    {
        ir_block_index block_index;
        unsigned operand_index;
    };

    typedef std::list< goto_fixup > goto_label;

    struct loop_gotos
    {
        goto_label* loop_break;
        goto_label* loop_continue;
    };

    struct materialize_goto
    {
        goto_label* const_true;
        goto_label* value_true;
        goto_label* const_false;
        goto_label* value_false;
    };

    // Visiting AST.
    ir_operand visit( ast_node_index node );
    ir_operand def_function( ast_node_index node, ir_operand omethod );
    void block_varenv( ast_node_index node );
    void visit_children( ast_node_index node );

    // Tests.
    void visit_test( ast_node_index node, goto_label* goto_true, goto_label* goto_false );
    void materialize( ast_node_index node, unsigned local_index, materialize_goto* jump );
    ir_operand comparison( ast_node_index op );

    // Rvals and unpacking
    unsigned rval_list( ast_node_index node, unsigned rvcount );
    unsigned assign_list( ast_node_index lval_init, ast_node_index lval_done, unsigned rvindex, unsigned unpack );
    ir_operand assign( ast_node_index lval, ir_operand rval );

    ir_operand expr_unpack( ast_node_index node, unsigned unpack );
    ir_operand call_op( ast_node_index node, ir_opcode opcode );

    // Constants.
    ir_operand number_operand( ast_node_index node );
    ir_operand string_operand( ast_node_index node );
    ir_operand selector_operand( ast_node_index node );

    // Emit ops.
    ir_operand emit( srcloc sloc, ir_opcode opcode, unsigned ocount );

    // Blocks and jumps.
    void goto_block( goto_label* label );
    ir_block_index new_block( srcloc sloc, ir_block_kind kind );
    ir_operand emit_jump( srcloc sloc, ir_opcode opcode, unsigned ocount, goto_label* goto_jump );
    ir_operand emit_test( srcloc sloc, ir_opcode opcode, unsigned ocount, goto_label* goto_true, goto_label* goto_false );
    ir_operand end_block( ir_operand jump );
    ir_block_index new_loop( ir_block_index loop_header );
    void end_loop( ir_block_index loop_header, goto_label* goto_loop );

    // Use/def for SSA construction.
    unsigned temporary();
    ir_operand def( srcloc sloc, unsigned local_index, ir_operand operand );
    ir_operand use( srcloc sloc, unsigned local_index );
    ir_operand search_def( ir_block_index block_index, unsigned local_index );
    void close_phi( ir_block_index block_index, unsigned local_index, unsigned phi_index );
    void seal_loop( ir_block_index loop_header );

    // Function under construction.
    report* _report;
    std::unique_ptr< ir_function > _f;

    // Operand stack.
    std::vector< ir_operand > _o;

    // Goto blocks.
    std::vector< loop_gotos > _loop_gotos;
    std::list< goto_fixup > _goto_block;
    ir_block_index _block_index;

    // Definitions per block.
    struct block_local { ir_block_index block_index; unsigned local_index; };
    struct block_local_hash : private std::hash< unsigned > { size_t operator () ( block_local bl ) const; };
    struct block_local_equal { bool operator () ( block_local a, block_local b ) const; };
    std::unordered_map< block_local, ir_operand, block_local_hash, block_local_equal > _defs;
    std::vector< ir_operand > _def_stack;

};

}

#endif
