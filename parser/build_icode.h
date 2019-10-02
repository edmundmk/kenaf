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



}

#endif
