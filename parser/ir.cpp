//
//  ir.cpp
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ir.h"
#include <string.h>
#include "ast.h"

namespace kf
{

ir_function::ir_function()
    :   ast( nullptr )
{
}

ir_function::~ir_function()
{
}

void ir_function::debug_print()
{
    for ( const auto& block : blocks )
    {
        block->debug_print();
    }
}

ir_oplist::ir_oplist()
    :   _ops( nullptr )
    ,   _body_size( 0 )
    ,   _head_size( 0 )
    ,   _watermark( 0 )
    ,   _capacity( 0 )
{
}

ir_oplist::~ir_oplist()
{
    free( _ops );
}

void ir_oplist::clear()
{
    _head_size = 0;
    _body_size = 0;
    _watermark = ( _capacity / 4 ) * 3;
}

unsigned ir_oplist::push_head( const ir_op& op )
{
    if ( _watermark + _head_size >= _capacity )
    {
        grow( false, true );
    }
    _ops[ _watermark + _head_size ] = op;
    return _head_size++;
}

unsigned ir_oplist::push_body( const ir_op& op )
{
    if ( _body_size >= _watermark )
    {
        grow( true, false );
    }
    _ops[ _body_size ] = op;
    return _body_size++;
}

void ir_oplist::grow( bool grow_body, bool grow_head )
{
    // Calculate updated sizes.
    size_t body_capacity = _watermark;
    size_t head_capacity = _capacity - _watermark;
    body_capacity = std::max< size_t >( body_capacity + ( grow_body ? body_capacity / 2 : 0 ), 8 );
    head_capacity = std::max< size_t >( head_capacity + ( grow_head ? head_capacity / 2 : 0 ), 8 );

    // Reallocate.
    _capacity = body_capacity + head_capacity;
    _ops = (ir_op*)realloc( _ops, _capacity * sizeof( ir_op ) );

    // Move head ops.
    memmove( _ops + body_capacity, _ops + _watermark, _head_size * sizeof( ir_op ) );
    _watermark = body_capacity;
}

ir_block::ir_block()
    :   loop_kind( IR_LOOP_NONE )
    ,   test_kind( IR_TEST_NONE )
    ,   block_index( IR_INVALID_INDEX )
    ,   function( nullptr )
    ,   loop( nullptr )
    ,   if_true( nullptr )
    ,   if_false( nullptr )
{
}

ir_block::~ir_block()
{
}

const char* const OPCODE_NAMES[] =
{
    [ IR_NOP        ] = "NOP",
    [ IR_REF        ] = "REF",
    [ IR_PHI        ] = "PHI",
    [ IR_PARAM      ] = "PARAM",
    [ IR_LENGTH     ] = "LENGTH",
    [ IR_NEG        ] = "NEG",
    [ IR_POS        ] = "POS",
    [ IR_BITNOT     ] = "BITNOT",
    [ IR_MUL        ] = "MUL",
    [ IR_DIV        ] = "DIV",
    [ IR_INTDIV     ] = "INTDIV",
    [ IR_MOD        ] = "MOD",
    [ IR_ADD        ] = "ADD",
    [ IR_SUB        ] = "SUB",
    [ IR_CONCAT     ] = "CONCAT",
    [ IR_LSHIFT     ] = "LSHIFT",
    [ IR_RSHIFT     ] = "RSHIFT",
    [ IR_ASHIFT     ] = "ASHIFT",
    [ IR_BITAND     ] = "BITAND",
    [ IR_BITXOR     ] = "BITXOR",
    [ IR_BITOR      ] = "BITOR",
    [ IR_GET_UPVAL  ] = "GET_UPVAL",
    [ IR_GET_KEY    ] = "GET_KEY",
    [ IR_GET_INDEX  ] = "GET_INDEX",
    [ IR_SUPEROF    ] = "SUPEROF",
    [ IR_CALL       ] = "CALL",
};

static void debug_print_op( ir_block* block, unsigned i )
{
    const ir_op& op = block->ops.at( i );
    printf( "    %s%04X %s", i & IR_HEAD_BIT ? "^" : "=", i & ~IR_HEAD_BIT, OPCODE_NAMES[ op.opcode ] );
    for ( unsigned o = 0; o < op.operand_count; ++o )
    {
        ir_operand operand = block->operands[ op.operands + o ];

        if ( o )
        {
            printf( "," );
        }

        switch ( operand.kind )
        {
        case IR_O_VALUE:
        {
            printf( " %s%04X", operand.index & IR_HEAD_BIT ? "^" : "=", operand.index & ~IR_HEAD_BIT );
            break;
        }

        case IR_O_PHI_BLOCK:
        {
            ir_operand opvalue = block->operands[ op.operands + ++o ];
            assert( opvalue.kind == IR_O_PHI_VALUE );
            ir_block* opblock = block->function->blocks.at( operand.index ).get();
            printf( " [%u]%s%04X", opblock->block_index, opvalue.index & IR_HEAD_BIT ? "^" : "=", opvalue.index & ~IR_HEAD_BIT );
            break;
        }

        case IR_O_PHI_VALUE:
        {
            assert( ! "malformed phi op" );
            break;
        }

        case IR_O_PARAM_INDEX:
        {
            const ast_local& local = block->function->ast->locals.at( operand.index );
            printf( " %.*s", (int)local.name.size(), local.name.data() );
            break;
        }

        case IR_O_UPVAL_INDEX:
        {
            printf( " UPVAL %u", operand.index );
            break;
        }

        case IR_O_INTEGER:
        {
            printf( " %i", ir_unpack_integer_operand( operand ) );
            break;
        }

        case IR_O_AST_NUMBER:
        {
            const ast_node& n = block->function->ast->nodes.at( operand.index );
            printf( " %f", n.leaf_number().n );
            break;
        }

        case IR_O_AST_STRING:
        {
            const ast_node& n = block->function->ast->nodes.at( operand.index );
            printf( " \"%.*s\"", (int)n.leaf_string().size, n.leaf_string().text );
            break;
        }

        case IR_O_AST_KEY:
        {
            const ast_node& n = block->function->ast->nodes.at( operand.index );
            printf( " KEY '%.*s'", (int)n.leaf_string().size, n.leaf_string().text );
            break;
        }

        case IR_O_FUNCTION:
        {
            printf( " FUNCTION %u", operand.index );
            break;
        }

        case IR_O_NULL:
        {
            printf( " NULL" );
            break;
        }

        case IR_O_TRUE:
        {
            printf( " TRUE" );
            break;
        }

        case IR_O_FALSE:
        {
            printf( " FALSE" );
            break;
        }
        }
    }

    printf( "\n" );
}

void ir_block::debug_print()
{
    printf( "[%u] BLOCK:\n", block_index );
    for ( unsigned i = 0; i < ops.head_size(); ++i )
    {
        debug_print_op( this, IR_HEAD_BIT | i );
    }
    for ( unsigned i = 0; i < ops.body_size(); ++i )
    {
        debug_print_op( this, i );
    }
}

}

