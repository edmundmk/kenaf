//
//  ir.h
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef IR_H
#define IR_H

/*
    -- Intermediate Representation

    This intermediate representation sits between the syntax tree and the
    bytecode.  The program is represented by a set of IR ops in a flat array.

    The ops describe a set of basic blocks, in program order.  Each block
    begins with a BLOCK_HEAD op, and ends with a jump.


    -- SSA Values

    The intermediate representation is SSA-like.  Each operation produces a
    value.  Values are used as operands by referencing the index of the op that
    produced it.

    phi functions that merge definitions for locals are generated during IR
    construction.  If only one definition of a local reaches a block, then a
    phi function with one operand (a ref) is still generated, as phi ops are
    used determine live ranges for locals.

    Phi functions are used as operands like any other op.


    -- Loop variables.

    Loop variables are the hidden variables used by the for each and for step
    loops.  They are live through the entire loop.  They are not represented
    explicitly in the IR, instead the special FOR instructions are used.


    -- Shortcut Branches

    Chained comparisons, logical operators, and conditional expressions can
    skip evaluation of some of their operands.

    These operators are not represented in the main control flow graph - as
    all the operands are expressions, and assignments are restricted, none will
    introduce new definitions of variables.  The definition of a variable
    reaching the start of the shortcut expression is the definition that will
    survive the expression.

    So instead of doing SSA construction through these structures, involving
    tracking definitions of temporaries, they are represented as internal
    branches inside a block.  Branches can only branch forward.

        B_AND test, jump
        B_CUT test, jump
            If test is true (B_AND) or false (B_CUT), branch to jump address,
            which must be later in the same block.  Does not produce a value.

        B_DEF link_cut, value, jump_phi
            The link_cut operand points to the B_CUT that skips this value.
            These links keep tests alive.  Branches to a B_PHI op with a value.

        B_PHI def, def, def, ..., value
            Each def is a B_DEF op providing an alternative value.  The last
            operand is the value to use if we didn't branch from a B_DEF.

    It's a bit complicated, but it reduces the complexity of the CFG and the
    amount of SSA variables we need to consider.


    -- Restrictions

    The IR has one major restriction.  Only one definition of each local can be
    live at any point.  This allows register allocation to allocate a single
    register for each local.  Code that constructs the IR must enforce this
    invariant.

*/

#include <stdint.h>
#include <assert.h>
#include "source.h"

namespace kf
{

struct ast_function;

struct ir_function;
struct ir_op;
struct ir_operand;
struct ir_number;
struct ir_string;

/*
    Op indexes are 24-bit.
*/

const unsigned IR_INVALID_INDEX = 0xFFFFFF;
const unsigned IR_INVALID_REGISTER = 0xFF;
const unsigned IR_INVALID_LOCAL = 0xFF;

/*
    Stores the intermediate representation for a function.
*/

struct ir_function
{
    ir_function();
    ~ir_function();

    void debug_print();

    ast_function* ast;
    std::vector< ir_op > ops;
    std::vector< ir_operand > operands;
    std::vector< ir_number > numbers;
    std::vector< ir_string > strings;
};

/*
    An op reads its operands and produces a result.

    Liveness information is stored for each op as the index of the last op in
    this block at which they are live.  If the result survives the block (it's
    assigned to a variable, or it's the result of a shortcut expression), then
    the op may appear in phi instructions in successor blocks.
*/

enum ir_opcode : uint8_t
{
    IR_NOP,

    // -- MUST MATCH AST NODES --
    IR_LENGTH,                  // #a
    IR_NEG,                     // -a
    IR_POS,                     // +a
    IR_BITNOT,                  // ~a
    IR_MUL,                     // a * b
    IR_DIV,                     // a / b
    IR_INTDIV,                  // a // b
    IR_MOD,                     // a % b
    IR_ADD,                     // a + b
    IR_SUB,                     // a - b
    IR_CONCAT,                  // a ~ b
    IR_LSHIFT,                  // a << b
    IR_RSHIFT,                  // a >> b
    IR_ASHIFT,                  // a ~>> b
    IR_BITAND,                  // a & b
    IR_BITXOR,                  // a ^ b
    IR_BITOR,                   // a | b
    // -- MUST MATCH AST NODES --

    // Comparisons.
    IR_EQ,                      // a == b
    IR_NE,                      // a != b
    IR_LT,                      // a < b, or b > a
    IR_LE,                      // a <= b, or b >= a
    IR_IS,                      // a is b, or not not a is b
    IR_NOT,                     // not a

    // Value stack.
    IR_L,                       // Maybe load, placeholder during construction.
    IR_LOAD,                    // Load value.

    // Other instructions.
    IR_GET_UPVAL,               // Get upval at index.
    IR_GET_KEY,                 // a.b
    IR_GET_INDEX,               // a[ b ]
    IR_SUPEROF,                 // superof( a )
    IR_APPEND,                  // a.append( b )

    // Stack top instructions.  If rcount is >1 then results must be selected.
    IR_CALL,                    // a( b, c, d ... ) ...
    IR_YIELD_FOR,               // yield for a( b, c, d ... ) ...
    IR_YIELD,                   // yield ... a, b, c ...
    IR_VARARG,                  // args ...
    IR_UNPACK,                  // a ...
    IR_EXTEND,                  // a.extend( b ... ) [rcount=0]

    // Select a result from a stack top instruction.
    IR_SELECT,                  // select( a ..., index )

    // Close upvals.
    IR_CLOSE_UPSTACK,           // index

    // Instructions operating on loop variables.
    IR_FOR_EACH_HEAD,           // g, i = generate( a )
    IR_FOR_EACH,                // a, b, c, [test] = for_each( &g, &i )
    IR_FOR_STEP_HEAD,           // i, limit, step = a, b, c
    IR_FOR_STEP,                // a, [test] = for_step( &i, &limit, &step )

    // Shortcut branches.
    IR_B_AND,                   // test, jump
    IR_B_CUT,                   // test, jump
    IR_B_DEF,                   // link_cut, value, jump_phi
    IR_B_PHI,                   // def, def, def, ..., value

    // Block instructions.
    IR_BLOCK_HEAD,              // block index
    IR_BLOCK_JUMP,              // jump
    IR_BLOCK_TEST,              // test, iftrue, iffalse
    IR_BLOCK_SHORTCUT,          // test, iftrue, iffalse, assigns local if test is false
    IR_BLOCK_FOR_TEST,          // test, iftrue, iffalse, test should be FOR_EACH or FOR_STEP
    IR_BLOCK_RETURN,            // value*
    IR_BLOCK_THROW,             // value
};

enum ir_operand_kind : uint8_t
{
    IR_O_NONE,                  // No operand.

    IR_O_OP,                    // Index of op.
    IR_O_JUMP,                  // Index of op to jump to (must be BLOCK_HEAD).

    IR_O_NULL,                  // null
    IR_O_TRUE,                  // true
    IR_O_FALSE,                 // false
    IR_O_NUMBER,                // Constant number.
    IR_O_STRING,                // Constant string.

    IR_O_LOCAL_INDEX,           // Index of local.
    IR_O_UPVAL_INDEX,           // Index of upval.
    IR_O_FUNCTION_INDEX,        // Index of function.
    IR_O_UPSTACK_INDEX,         // Upstack index.
};

struct ir_op
{
    ir_op()
        :   opcode( IR_NOP )
        ,   r( IR_INVALID_REGISTER )
        ,   stack_top( IR_INVALID_REGISTER )
        ,   unpack( 0 )
        ,   ocount( 0 )
        ,   oindex( IR_INVALID_INDEX )
        ,   local( IR_INVALID_LOCAL )
        ,   live_range( IR_INVALID_INDEX )
        ,   sloc( 0 )
    {
    }

    ir_opcode opcode;           // Opcode.

    uint8_t r;                  // Allocated register.
    uint8_t stack_top;          // Stack top at this op.
    uint8_t unpack;             // Number of results requested.

    unsigned ocount : 8;        // Number of operands.
    unsigned oindex : 24;       // Index into operand list.

    unsigned local : 8;         // Index of local result is assigned to.
    unsigned live_range : 24;   // Last use in this block.

    srcloc sloc;                // Source location.
};

struct ir_operand
{
    ir_operand_kind kind : 8;    // Operand kind.
    unsigned index : 24;            // Index of op used as result.
};

struct ir_number
{
    double n;
};

struct ir_string
{
    const char* text;
    size_t size;
};

}

#endif

