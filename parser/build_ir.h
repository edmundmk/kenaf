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
    -- IR Building

    To build the intermediate representation, we traverse the AST.  Each value
    in an expression is pushed onto an evaluation stack.  Operations cause ops
    to be emitted into the current block, and the result is a new value that
    references the op, which is pushed onto the stack.

    The evaluation stack can also hold literal values, which are only emitted
    when absolutely necessary (preferably as the operand to another op).  So
    a simple form of constant folding is performed at this stage.


    -- Assignment

    kenaf's assignment statement has the following semantics:

        a, b, c = x, y, z

        t0 <- evaluate x
        t1 <- evaluate y
        t2 <- evaluate z
        evaluate( a ) <- t0
        evaluate( b ) <- t1
        evaluate( c ) <- t2

    We must be careful to ensure we preserve our intermediate representation's
    invariant that there is only one live definition of each local at any time.


    -- SSA Construction

    To generate live ranges for locals, we perform SSA construction by finding
    the definitions which reach each use of the local.

    The algorithm is based on this paper:

        http://www.cdl.uni-saarland.de/papers/bbhlmz13cc.pdf

    Whenever a local is pushed onto the evaluation stack, we perform a search
    for the definition in predecessor blocks.  This extends the live ranges of
    those defnitions which reach the current point.

*/

#include <utility>
#include <functional>
#include <unordered_map>
#include "ir.h"
#include "ast.h"

template <> struct std::hash< std::pair< unsigned, kf::ir_block* > >
    :   private std::hash< unsigned >, private std::hash< kf::ir_block* >
{
    inline size_t operator () ( std::pair< unsigned, kf::ir_block* > key ) const
    {
        return std::hash< unsigned >::operator () ( key.first ) ^ std::hash< kf::ir_block* >::operator () ( key.second );
    }
};

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

    struct build_loop
    {
        ir_block* loop;
        ir_block* continue_block;
        ir_block* break_block;
    };

    // AST traversal.
    node_index child_node( node_index node );
    node_index next_node( node_index node );

    // Visit AST node, producing a value.
    ir_operand visit( node_index node );

    // Visit child nodes and evaluate them.
    struct operand_pair { ir_operand o[ 2 ]; };
    operand_pair visit_pair( node_index node, node_index last );
    unsigned visit_eval( node_index node, node_index last );
    void visit_list( node_index node, node_index last );

    // Check if operands are literals.
    bool check_number( ir_operand operand, double* n );
    bool check_number( operand_pair operands, double n[ 2 ] );

    // Control flow graph.
    ir_block* make_block();
    ir_block* jump_block( ir_block* block = nullptr );
    ir_block* link_next_block( ir_block* block, ir_block* next );
    ir_block* link_fail_block( ir_block* block, ir_block* fail );
    ir_block* test( ir_operand operand );

    // Emit ops.
    unsigned op( srcloc sloc, ir_opcode opcode, ir_operand* o, unsigned ocount, bool head = false );

    // Variable use/def.
    void def( unsigned local_index, ir_block* block, unsigned op_index );

    ast_function* _ast_function;
    std::unique_ptr< ir_function > _ir_function;

    // The location of all defs of variables.
    std::unordered_map< std::pair< unsigned, ir_block* >, unsigned > _def_map;

    // Evaluation stack.
    std::vector< ir_operand > _v;

    // Loop hierarchy.  First element is the function's entry block.
    std::vector< build_loop > _loop_stack;

    // Block under construction.
    ir_block* _block;

};

}

#endif
