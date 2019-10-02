//
//  icode.h
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef ICODE_H
#define ICODE_H

#include <stdint.h>
#include "source.h"

namespace kf
{

/*
    This is an intermediate representation that sits between the syntax tree
    and the bytecode.  We are trying to produce good bytecode.  icode is
    designed to help us do this.

    First, constant folding.  Calculations involving literal values should be
    replaced by the result.

    Second, register allocation.  Performing 'real' register allocation has
    two main benefits:

      - The virtual machine we are targeting is a register machine, but one
        where arguments to calls must be set up in adjacent registers used as a
        stack.  We can eliminate one class of unecessary moves by generating
        values in the register they need to end up in.

      - Running 'real' register allocation using liveness information allows us
        to make much better use of registers, reducing the size of the call
        stack and again allowing elimination of some moves.

    The intermediate representation is SSA-like, but with one major restriction
    which make things simpler:

      - All definitions of a local variable will eventually map to the same
        register.  Code that constructs the intermediate representation must
        guarantee that an old definition of a variable is dead before
        introducing a new definition.  Maintaining this guarantee might involve
        *inserting* an extra copy to a temporary, to preserve the old value.

    Since our language has no goto, the control flow graph stays simple.  The
    block structure of the original code is preserved - loop headers are
    identified explicitly, including the type of loop.

    Liveness information is constructed along with the intermediate
    representation.  Temporary results typically die where they are used.  And
    both SSA construction and liveness analysis require the same kind of
    backwards search through the CFG - effectively we get liveness information
    'for free' when we perform SSA construction.
*/

struct syntax_function;

class icode_op_list;
struct icode_op;
struct icode_operand;
struct icode_block;
struct icode_function;

const unsigned ICODE_INVALID_INDEX = ~(unsigned)0;
const uint8_t ICODE_INVALID_R = 0xFF;

/*
    An op list is a flat array of ops, divided into two halves, the head and
    the body.  Ops are referenced by index.  Indexes into the head have the
    top bit set.  Indexes into the body have the top bit clear.

    The op list reallocates when either the head or body half grows too large.
    This allows clients to insert ops at both the beginning and end of a block.
*/

class icode_op_list
{
public:

    icode_op_list();
    ~icode_op_list();

    size_t head_size() const;
    size_t body_size() const;

    void clear();
    unsigned push_head( const icode_op& op );
    unsigned push_head( const icode_operand& operand );
    unsigned push_body( const icode_op& op );
    unsigned push_body( const icode_operand& operand );

    const icode_op& operator [] ( size_t i ) const;
    const icode_op& at( size_t i ) const;
    icode_op& operator [] ( size_t i );
    icode_op& at( size_t i );

private:

    icode_op* _ops;
    size_t _head_size;
    size_t _total_size;
    size_t _capacity;

};

/*
    An op reads its operands and produces a result.

    Liveness information is stored for each op as the index of the last op in
    this block at which they are live.  If the result survives the block (it's
    assigned to a variable, or it's the result of a shortcut expression), then
    the op may appear in phi instructions in successor blocks.
*/

enum icode_opcode : uint8_t
{


};

enum icode_operand_kind
{
    IR_OPERAND_VALUE,           // Reference to result of op in this block.
    IR_OPERAND_PHI_VALUE,       // Reference to result of op in predecessor block.
    IR_OPERAND_LITERAL_NUMBER,  // Literal number value.
    IR_OPERAND_LITERAL_STRING,  // Literal string value.
};

struct icode_op
{
    icode_opcode opcode;        // Opcode.
    uint8_t operand_count;      // Number of operands which follow this op.
    uint8_t variable;           // Local variable this result is assigned to.
    uint8_t scratch0;

    uint8_t r;                  // Register allocated to this result.
    uint8_t stack_top;          // 'stack' top at this op.
    uint8_t temp_r;             // Temporary register for literal loading.
    uint8_t scratch1;

    srcloc sloc;                // Source location that generated this op.
    unsigned live_range;        // Last use in this block.

    const icode_operand& operator [] ( size_t i ) const;
    const icode_operand& operand( size_t i ) const;
    icode_operand& operator [] ( size_t i );
    icode_operand& operand( size_t i );
};

struct icode_operand
{
    icode_operand_kind kind;    // Kind of operand.
    union
    {
        unsigned index;         // Index of op.
        unsigned size;          // Size of string literal.
    };
    union
    {
        icode_block* block;     // For phi, predecessor block containing op.
        double n;               // Literal number.
        const char* text;       // String literal text.
    };
};

static_assert( sizeof( icode_operand ) <= sizeof( icode_op ) );

/*
    A block is a sequence of instructions without branches.
*/

enum icode_loop_kind : unsigned short
{
    IR_LOOP_NONE,               // Not a loop header.
    IR_LOOP_FOR_STEP,           // Loop header of for i = start : stop : step do
    IR_LOOP_FOR_EACH,           // Loop header of for i : generator do
    IR_LOOP_WHILE,              // Loop header of while loop.
    IR_LOOP_REPEAT,             // Loop header of repeat/until loop.
};

enum icode_test_kind : unsigned short
{
    IR_TEST_NONE,               // No test.
    IR_TEST_IF,                 // Block ends with an if test between two sucessors.
};

struct icode_block
{
    icode_block();
    ~icode_block();

    icode_loop_kind loop_kind;
    icode_test_kind test_kind;

    icode_block* loop;          // Loop containing this block.
    icode_block* if_true;       // Successor if test is true.
    icode_block* if_false;      // Successor if test is false.

    icode_op_list ops;          // List of ops.
};

/*
    Stores the intermediate representation for a function.
*/

struct icode_function
{
    icode_function();
    ~icode_function();

    void debug_print();

    syntax_function* ast;
    std::vector< std::unique_ptr< icode_block > > blocks;
};

}

#endif

