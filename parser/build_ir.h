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

    enum goto_kind
    {
        GOTO_ELSE,
        GOTO_ENDIF,
        GOTO_BREAK,
        GOTO_CONTINUE,
        GOTO_MAX,
    };

    struct goto_fixup
    {
        ir_block_index block_index;
        unsigned operand_index;
    };

    struct goto_stack
    {
        std::vector< goto_fixup > fixups;
        unsigned index;
    };

    // AST traversal.
    node_index child_node( node_index node );
    node_index next_node( node_index node );

    // Visiting AST.
    ir_operand visit( node_index node );
    void visit_children( node_index node );

    // Rvals and unpacking
    unsigned rval_list( node_index node, unsigned rvcount );
    void assign( node_index lval, ir_operand rval );

    ir_operand expr_unpack( node_index node, unsigned unpack );
    ir_operand call_op( node_index node, ir_opcode opcode );

    // Constants.
    ir_operand number_operand( node_index node );
    ir_operand string_operand( node_index node );

    // Emit ops.
    ir_operand emit( srcloc sloc, ir_opcode opcode, unsigned ocount );
    void close_upstack( node_index node );

    // Pins.
    ir_operand pin( srcloc sloc, ir_operand value );
    ir_operand ignore_pin( ir_operand operand );
    void fix_local_pins( unsigned local );
    void fix_upval_pins();

    // Structured gotos.
    struct goto_scope { goto_kind kind; unsigned index; };
    goto_scope goto_open( srcloc sloc, goto_kind kind );
    void goto_branch( goto_scope scope );
    void goto_block( goto_scope scope );

    // Blocks and jumps.
    ir_block_index new_block( srcloc sloc, ir_block_kind kind );
    ir_operand emit_jump( srcloc sloc, ir_opcode opcode, unsigned ocount, goto_kind goto_kind );
    ir_operand emit_test( srcloc sloc, ir_opcode opcode, unsigned ocount, goto_kind goto_true, goto_kind goto_false );
    ir_operand end_block( ir_operand jump );
    ir_block_index new_loop( ir_block_index loop_header );
    void end_loop( ir_block_index loop_header, goto_scope scope );

    // Use/def for SSA construction.
    ir_operand use( srcloc sloc, unsigned local );
    ir_operand search_def( ir_block_index block_index, unsigned local );
    void close_phi( ir_block_index block_index, unsigned local, unsigned phi_index );
    void seal_loop( ir_block_index loop_header );
    void def( srcloc sloc, unsigned local, ir_operand operand );

    // Function under construction.
    source* _source;
    std::unique_ptr< ir_function > _f;

    // Operand stack.
    std::vector< ir_operand > _o;

    // Block construction and branch stacks.
    goto_stack _goto_stacks[ GOTO_MAX ];
    std::vector< ir_block_index > _loop_stack;
    ir_block_index _block_index;

    // Definitions per block.
    struct block_local { ir_block_index block_index; unsigned local; };
    struct block_local_hash : private std::hash< unsigned > { size_t operator () ( block_local bl ) const; };
    struct block_local_equal { bool operator () ( block_local a, block_local b ) const; };
    std::unordered_map< block_local, ir_operand, block_local_hash, block_local_equal > _defs;
    std::vector< ir_operand > _def_stack;

};

}

#endif
