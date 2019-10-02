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
#include <assert.h>
#include <stdexcept>
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

struct icode_function;
struct icode_block;
class icode_oplist;
struct icode_op;
struct icode_operand;

/*
    Op indexes are 24-bit.
*/

const unsigned IR_INVALID_INDEX = 0xFFFFFF;
const unsigned IR_INVALID_REGISTER = 0xFF;
const unsigned IR_TEMPORARY = 0xFF;

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

/*
    An op list is a flat array of ops, divided into two halves, the head and
    the body.  Ops are referenced by index.  Indexes into the head have the
    top bit set.  Indexes into the body have the top bit clear.

    The op list reallocates when either the head or body half grows too large.
    This allows clients to insert ops at both the beginning and end of a block.

    In the actual memory the head is stored *after* the body, to make the more
    common case of accessing the body simpler.
*/

const unsigned IR_HEAD_BIT = 0x800000;

class icode_oplist
{
public:

    icode_oplist();
    ~icode_oplist();

    unsigned head_size() const;
    unsigned body_size() const;

    void clear();
    unsigned push_head( const icode_op& op );
    unsigned push_body( const icode_op& op );

    const icode_op& operator [] ( unsigned i ) const;
    const icode_op& at( unsigned i ) const;
    icode_op& operator [] ( unsigned i );
    icode_op& at( unsigned i );

private:

    static const unsigned INDEX_MASK = 0x7FFFFF;

    icode_oplist( icode_oplist& ) = delete;
    icode_oplist& operator = ( icode_oplist& ) = delete;

    void grow( bool grow_body, bool grow_head );

    icode_op* _ops;
    size_t _body_size;
    size_t _head_size;
    size_t _watermark;
    size_t _capacity;

};

/*
    A block is a sequence of instructions without branches.
*/

enum icode_loop_kind : unsigned
{
    IR_LOOP_NONE,                   // Not a loop header.
    IR_LOOP_FOR_STEP,               // Loop header of for i = start : stop : step do
    IR_LOOP_FOR_EACH,               // Loop header of for i : generator do
    IR_LOOP_WHILE,                  // Loop header of while loop.
    IR_LOOP_REPEAT,                 // Loop header of repeat/until loop.
};

enum icode_test_kind : unsigned
{
    IR_TEST_NONE,                   // No test.  Successor is if_true.
    IR_TEST_IF,                     // Block ends with an if test between two sucessors.
};

struct icode_block
{
    icode_block();
    ~icode_block();

    void debug_print();

    unsigned loop_kind : 4;  // Loop kind.
    unsigned test_kind : 4;  // Test kind.
    unsigned block_index : 24;      // Index in function's list of blocks.

    icode_function* function;       // Function containing this block.
    icode_block* loop;              // Loop containing this block.
    icode_block* if_true;           // Successor if test is true.
    icode_block* if_false;          // Successor if test is false.

    // List of predecessor block indices.
    std::vector< unsigned > predecessor_blocks;

    // Oplist and operands.
    icode_oplist ops;
    std::vector< icode_operand > operands;
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
    IR_NOP,

    // Head
    IR_REF,                     // Reference to op in predecessor block.
    IR_PHI,                     // phi function.
    IR_PARAM,                   // Parameter.

    // Body.
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

    IR_GET_UPVAL,               // Get upval at index.
    IR_GET_KEY,                 // a.b
    IR_GET_INDEX,               // a[ b ]
    IR_SUPEROF,                 // Find prototype of value.
    IR_CALL,                    // a( b, c, d .. )

};

enum icode_operand_kind : uint8_t
{
    IR_O_VALUE,                 // Index of op in this block.
    IR_O_PHI_BLOCK,             // Index of block for phi/ref operand.
    IR_O_PHI_VALUE,             // Index of op in phi block for phi/ref operand.
    IR_O_PARAM_INDEX,           // Index of parameter local.
    IR_O_UPVAL_INDEX,           // Index of upval.
    IR_O_INTEGER,               // Small integer encoded directly.
    IR_O_AST_NUMBER,            // Number value in AST node.
    IR_O_AST_STRING,            // String value in AST node.
    IR_O_AST_KEY,               // Key string in AST node.
    IR_O_FUNCTION,              // Function, index into syntax tree.
    IR_O_NULL,                  // null
    IR_O_TRUE,                  // true
    IR_O_FALSE,                 // false
};

struct icode_op
{
    icode_op() : opcode( IR_NOP ), r( IR_INVALID_REGISTER ),
        stack_top( IR_INVALID_REGISTER ), temp_r( IR_INVALID_REGISTER ),
        operand_count( 0 ), operands( IR_INVALID_INDEX ),
        variable( IR_TEMPORARY ), live_range( IR_INVALID_INDEX ), sloc( 0 ) {}

    icode_opcode opcode;        // Opcode.
    uint8_t r;                  // Allocated register.
    uint8_t stack_top;          // Stack top at this op.
    uint8_t temp_r;             // Temporary register for literal loading.

    unsigned operand_count : 8; // Number of operands which follow this op.
    unsigned operands : 24;     // Index into block's operand list.

    unsigned variable : 8;      // Index of variable result is assigned to.
    unsigned live_range : 24;   // Last use in this block.

    srcloc sloc;                // Source location.
};

struct icode_operand
{
    icode_operand_kind kind : 8;    // Operand kind.
    unsigned index : 24;            // Index of op used as result.
};

inline icode_operand icode_pack_integer_operand( int8_t i )
{
    return { IR_O_INTEGER, (unsigned)(int)i };
}

inline int8_t icode_unpack_integer_operand( icode_operand operand )
{
    return (int8_t)(int)(unsigned)operand.index;
}

/*
*/

inline unsigned icode_oplist::head_size() const
{
    return _head_size;
}

inline unsigned icode_oplist::body_size() const
{
    return _body_size;
}

inline const icode_op& icode_oplist::operator [] ( unsigned i ) const
{
    if ( ( i & IR_HEAD_BIT ) == 0 )
    {
        assert( i < _body_size );
        return _ops[ i ];
    }
    else
    {
        i &= INDEX_MASK;
        assert( i < _head_size );
        return _ops[ _watermark + i ];
    }
}

inline const icode_op& icode_oplist::at( unsigned i ) const
{
    if ( ( i & IR_HEAD_BIT ) == 0 )
    {
        if ( i >= _body_size ) throw std::out_of_range( "op index is out of range" );
        return _ops[ i ];
    }
    else
    {
        i &= INDEX_MASK;
        if ( i >= _head_size ) throw std::out_of_range( "op index is out of range" );
        return _ops[ _watermark + i ];
    }
}

inline icode_op& icode_oplist::operator [] ( unsigned i )
{
    if ( ( i & IR_HEAD_BIT ) == 0 )
    {
        assert( i < _body_size );
        return _ops[ i ];
    }
    else
    {
        i &= INDEX_MASK;
        assert( i < _head_size );
        return _ops[ _watermark + i ];
    }
}

inline icode_op& icode_oplist::at( unsigned i )
{
    if ( ( i & IR_HEAD_BIT ) == 0 )
    {
        if ( i >= _body_size ) throw std::out_of_range( "op index is out of range" );
        return _ops[ i ];
    }
    else
    {
        i &= INDEX_MASK;
        if ( i >= _head_size ) throw std::out_of_range( "op index is out of range" );
        return _ops[ _watermark + i ];
    }
}
}

#endif

