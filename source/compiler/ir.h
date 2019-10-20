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

    The ops describe a set of basic blocks, in program order.  Because we do
    not support goto, the control flow graph is reducible and program order is
    a valid depth first traversal of the CFG, with dominators preceding the
    nodes they dominate.


    -- Blocks

    Each block begins with a BLOCK op, and ends with a jump.

    The BLOCK op references a block description in the blocks array of the
    function.  Among other information, the block description links to the
    first phi op.

    Phi ops gather local definitions at entry to the block.  Although phi ops
    are emitted intermixed with other instructions, they are linked together in
    a list, and are conceptually part of the block header.

    Phi ops can be one of the following:

        PHI (local) [phi_link], def, def, def
            Lists all definitions that reach this block.  In unreachable code
            the list of definitions may be empty.

        PHI_OPEN (local) [phi_link]
            During IR construction, phi ops in unsealed loops are represented
            by PHI_OPEN ops.

    The link to the next phi op in the block is stored in the sloc field.


    -- Loop variables.

    The hidden variables (g/i for FOR_EACH, i/l/s for FOR_STEP) of a for loop
    are represented in the IR as a single variable.  Liveness information is
    generated as for any other variable.


    -- Shortcut Branches

    Chained comparisons, logical operators, and conditional expressions can
    skip evaluation of some of their operands.

    These operators are not represented in the main control flow graph.  As
    all the operands are expressions, and assignments are restricted, new
    definitions of variables cannot be created inside these expressions.  The
    definition of a variable reaching the start of the shortcut expression is
    the definition that will survive the expression.

    So instead of doing CFG and SSA construction for these structures - which
    would involve defining temporary variables - we represent them as internal
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


    -- SSA Form

    The IR is a kind of SSA form, but with some major restrictions:

        - Each value live in a block must have a op which defines its live
          range, whether that is a real op or a PHI/REF in the block header.
          Only PHI/REF ops reference ops in other blocks.  This simplifies
          the data structures required for liveness analysis.

        - Only explicitly declared local variables participate in SSA
          construction (and only those which are not captured).  Shortcut
          B_DEF/B_PHI are handled as special cases, keeping the number of
          variables low and reducing the number of PHI/REF ops.

        - Only one definition of each local variable is live at any point.

    These properties ensure that the register allocator has all the information
    it needs in order to allocate a single register for each local.
*/

#include <stdint.h>
#include <assert.h>
#include "index_vector.h"
#include "source.h"

namespace kf
{

struct ast_function;

struct ir_function;
struct ir_op;
struct ir_operand;
struct ir_block;
struct ir_constant;
struct ir_selector;

typedef unsigned ir_block_index;

/*
    Op indexes are 24-bit.
*/

const unsigned IR_INVALID_INDEX = 0xFFFFFF;
const unsigned IR_INVALID_LOCAL = 0xFEFF;
const unsigned IR_INVALID_REGISTER = 0xFF;
const unsigned IR_LIVE_STICKY = 0xFF;
const unsigned IR_UNPACK_ALL = 0xFF;

/*
    Stores the intermediate representation for a function.
*/

struct ir_function
{
    ir_function();
    ~ir_function();

    void debug_print() const;
    void debug_print_phi_graph() const;

    ast_function* ast;

    // Main IR structures.
    index_vector< ir_op, 0xFFFFFF > ops;
    index_vector< ir_operand, 0xFFFFFF > operands;
    index_vector< ir_block, 0xFFFFFF > blocks;
    index_vector< ir_block_index, 0xFFFFFF > preceding_blocks;

    // Constant numbers and strings.
    index_vector< ir_constant, 0xFFFFFF > constants;
    index_vector< ir_selector, 0xFFFFFF > selectors;
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

    // Values.
    IR_PARAM,                   // Parameter placeholder.
    IR_CONST,                   // Constant.
    IR_MOV,                     // Move between values.

    // Comparisons.
    IR_EQ,                      // a == b
    IR_NE,                      // a != b
    IR_LT,                      // a < b, or b > a
    IR_LE,                      // a <= b, or b >= a
    IR_IS,                      // a is b
    IR_NOT,                     // not a

    // Other instructions.
    IR_GET_GLOBAL,              // Get global.
    IR_GET_KEY,                 // a.b
    IR_SET_KEY,                 // a.b = c
    IR_GET_INDEX,               // a[ b ]
    IR_SET_INDEX,               // a[ b ] = c
    IR_NEW_ENV,                 // count
    IR_GET_ENV,                 // $varenv/outenv_index env_index
    IR_SET_ENV,                 // $varenv/outenv_index env_index value
    IR_NEW_OBJECT,              // def
    IR_NEW_ARRAY,               // []
    IR_NEW_TABLE,               // {}
    IR_NEW_FUNCTION,            // function, varenv/outenv_index*
    IR_SUPER,                   // super( self ), performs late binding.
    IR_APPEND,                  // a.append( b )

    // Stack top instructions.  If rcount is >1 then results must be selected.
    IR_CALL,                    // a( b, c, d ... ) ...
    IR_YCALL,                   // yield for a( b, c, d ... ) ...
    IR_YIELD,                   // yield ... a, b, c ...
    IR_VARARG,                  // args ...
    IR_UNPACK,                  // a ...
    IR_EXTEND,                  // a.extend( b ... ) [rcount=0]

    // Select a result from a stack top instruction.
    IR_SELECT,                  // select( a ..., index )

    // Shortcut branches.
    IR_B_AND,                   // test, jump
    IR_B_CUT,                   // test, jump
    IR_B_DEF,                   // link_cut, value, jump_phi
    IR_B_PHI,                   // def, def, def, ..., value

    // Block and jump instructions.
    IR_BLOCK,                   // Block header.
    IR_JUMP,                    // Jump to new block.
    IR_JUMP_TEST,               // test, iftrue, iffalse
    IR_JUMP_THROW,              // value
    IR_JUMP_RETURN,             // value*
    IR_JUMP_FOR_EGEN,           // g, jump /* $for_each */
    IR_JUMP_FOR_EACH,           // $for_each, loop, break
    IR_JUMP_FOR_SGEN,           // start, limit, step, jump /* $for_step */
    IR_JUMP_FOR_STEP,           // $for_step, loop, break

    // For loop variables.
    IR_FOR_EACH_ITEMS,          // $for_each, results are generated items
    IR_FOR_STEP_INDEX,          // $for_step, result is for step index

    // Phi instructions.
    IR_PHI,                     // Phi function.
    IR_PHI_OPEN,                // Open phi function in unclosed loop.
    IR_REF,                     // Value reference.
};

enum ir_operand_kind : uint8_t
{
    IR_O_NONE,          // No operand.

    IR_O_OP,            // Index of op.
    IR_O_PIN,           // Index of pin op.
    IR_O_SELECT,        // Index of selected result.

    IR_O_BLOCK,         // Index of block in function's blocks array.
    IR_O_JUMP,          // Index of op to jump to.

    IR_O_NULL,          // null
    IR_O_TRUE,          // true
    IR_O_FALSE,         // false
    IR_O_NUMBER,        // Constant number.
    IR_O_STRING,        // Constant string.
    IR_O_IMMEDIATE,     // 8-bit signed immediate.
    IR_O_SELECTOR,      // Constant selector.

    IR_O_LOCAL,         // Index of local.
    IR_O_OUTENV,        // Index of outenv
    IR_O_ENVSLOT,       // Index of slot in varenv or outenv.
    IR_O_FUNCTION,      // Index of function.
};

enum ir_block_kind : uint8_t
{
    IR_BLOCK_NONE,
    IR_BLOCK_BASIC,
    IR_BLOCK_LOOP,
    IR_BLOCK_UNSEALED,
};

enum ir_live_value_kind : uint8_t
{
    IR_LV_LOCAL,
    IR_LV_FOR_INDEX,
    IR_LV_FOR_LIMIT,
    IR_LV_FOR_STEP,
    IR_LV_FOR_GENERATOR,
    IR_LV_FOR_GEN_INDEX,
};

struct ir_op
{
    ir_op()
        :   opcode( IR_NOP )
        ,   mark( false )
        ,   s( IR_INVALID_REGISTER )
        ,   localu( IR_INVALID_LOCAL )
        ,   ocount( 0 )
        ,   oindex( IR_INVALID_INDEX )
        ,   r( IR_INVALID_REGISTER )
        ,   live_range( IR_INVALID_INDEX )
        ,   sloc( 0 )
    {
    }

    unsigned opcode : 7;        // Opcode.
    unsigned mark : 1;          // Mark bit.
    unsigned s : 8;             // Stack top or liveness count.
    unsigned localu : 16;       // Local variable index or unpack count.

    unsigned ocount : 8;        // Number of operands.
    unsigned oindex : 24;       // Index into operand list.

    unsigned r : 8;             // Allocated register.
    unsigned live_range : 24;   // Last use in this block.

    union
    {
        srcloc sloc;            // Source location.
        unsigned phi_next;      // Next phi in block.
    };

    unsigned local() const              { return std::min< unsigned >( localu, IR_INVALID_LOCAL ); }
    unsigned unpack() const             { return localu >= 0xFF00 ? localu & 0xFF : 1; }
    void set_local( unsigned local )    { localu = local; }
    void set_unpack( unsigned unpack )  { localu = 0xFF00 | unpack; }
};

struct ir_operand
{
    ir_operand_kind kind : 8;    // Operand kind.
    unsigned index : 24;         // Index of op used as result.
};

struct ir_block
{
    ir_block()
        :   kind( IR_BLOCK_BASIC )
        ,   lower( IR_INVALID_INDEX )
        ,   mark( 0 )
        ,   reachable( false )
        ,   upper( IR_INVALID_INDEX )
        ,   preceding_lower( IR_INVALID_INDEX )
        ,   preceding_upper( IR_INVALID_INDEX )
        ,   phi_head( IR_INVALID_INDEX )
        ,   phi_tail( IR_INVALID_INDEX )
    {
    }

    ir_block_kind kind : 8;     // Block kind.
    unsigned lower : 24;        // Index of first op in block.
    unsigned mark : 7;          // Analysis mark.
    unsigned reachable : 1;     // Is this block reachable?
    unsigned upper : 24;        // Index past last op in block.
    unsigned preceding_lower;   // Index of first block in preceding_blocks.
    unsigned preceding_upper;   // Index past last preceding block.
    unsigned phi_head;          // Index of first phi op in block.
    unsigned phi_tail;          // Index of last phi op in block.
};

struct ir_constant
{
    explicit ir_constant( double n ) : text( nullptr ), n( n ) {}
    explicit ir_constant( const char* text, size_t size ) : text( text ), size( size ) {}

    const char* text;
    union { size_t size; double n; };
};

struct ir_selector
{
    const char* text;
    size_t size;
};

}

#endif

