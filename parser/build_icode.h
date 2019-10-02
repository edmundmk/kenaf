//
//  build_icode.h
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef BUILD_ICODE_H
#define BUILD_ICODE_H

#include <utility>
#include <functional>
#include <unordered_map>
#include "icode.h"

template <> struct std::hash< std::pair< unsigned, kf::icode_block* > >
    :   private std::hash< unsigned >, private std::hash< kf::icode_block* >
{
    inline size_t operator () ( std::pair< unsigned, kf::icode_block* > key ) const
    {
        return std::hash< unsigned >::operator () ( key.first ) ^ std::hash< kf::icode_block* >::operator () ( key.second );
    }
};

namespace kf
{

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

    But we can reduce the lifetime of temporaries by attempting to move the
    evaluation of the left hand side directly after the evaluation of the right
    hand side, e.g.:

        t0 <- evaluate x
        evaluate( a ) <- t0
        t1 <- evaluate y
        evaluate( b ) <- t1
        t2 <- evaluate z
        evaluate( c ) <- t2

    Moving a left hand side is only possible if:

      - expressions y and z do not clobber any locals used in a
      - expression a does not clobber any locals used in y and/or z

    Clobbering a local happens when:

      - the local is assigned.
      - the local is an upval, and any function is called.

    Because assignment is a statement, the only assignment which is possible
    is the assignment to a itself.

    In this pass we finally check if the number of values on either side of
    the assignment is equal.  If the right hand side ends in an unpack, there
    may be any number of extra values on the left hand side.


    -- SSA Construction

    To generate live ranges for locals, we perform SSA construction by finding
    the definitions which reach each use of the local.

    Whenever a local is pushed onto the evaluation stack, we perform a search
    for the definition in predecessor blocks.  This extends the live ranges of
    those defnitions which reach the current point.

    The algorithm is based on this paper:

        http://www.cdl.uni-saarland.de/papers/bbhlmz13cc.pdf

    We must be careful to ensure we preserve our intermediate representation's
    invariant that there is only one live definition of each local at any time.

*/

class build_icode
{
public:

    build_icode();
    ~build_icode();

    std::unique_ptr< icode_function > build( syntax_function* function );

private:

    void visit( unsigned ast_index );

    unsigned op( srcloc sloc, icode_opcode opcode, unsigned operand_count, bool head = false );
    void def( unsigned local_index, icode_block* block, unsigned op_index );

    syntax_function* _ast_function;
    std::unique_ptr< icode_function > _ir_function;
    std::unordered_map< std::pair< unsigned, icode_block* >, unsigned > _def_map;
    icode_block* _block;

    std::vector< icode_operand > _eval;

};

}

#endif