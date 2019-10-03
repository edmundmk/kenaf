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

const char* const OPCODE_NAMES[] =
{
    [ IR_NOP            ] = "NOP",
    [ IR_BLOCK_HEAD     ] = "BLOCK_HEAD",
    [ IR_BLOCK_TEST     ] = "BLOCK_TEST",
    [ IR_BLOCK_JUMP     ] = "BLOCK_JUMP",
    [ IR_BLOCK_RETURN   ] = "BLOCK_RETURN",
    [ IR_BLOCK_THROW    ] = "BLOCK_THROW",
    [ IR_CLOSE_UPSTACK  ] = "CLOSE_UPSTACK",
    [ IR_GENERATE       ] = "GENERATE",
    [ IR_FOR_EACH       ] = "FOR_EACH",
    [ IR_FOR_STEP       ] = "FOR_STEP",
    [ IR_LENGTH         ] = "LENGTH",
    [ IR_NEG            ] = "NEG",
    [ IR_POS            ] = "POS",
    [ IR_BITNOT         ] = "BITNOT",
    [ IR_MUL            ] = "MUL",
    [ IR_DIV            ] = "DIV",
    [ IR_INTDIV         ] = "INTDIV",
    [ IR_MOD            ] = "MOD",
    [ IR_ADD            ] = "ADD",
    [ IR_SUB            ] = "SUB",
    [ IR_CONCAT         ] = "CONCAT",
    [ IR_LSHIFT         ] = "LSHIFT",
    [ IR_RSHIFT         ] = "RSHIFT",
    [ IR_ASHIFT         ] = "ASHIFT",
    [ IR_BITAND         ] = "BITAND",
    [ IR_BITXOR         ] = "BITXOR",
    [ IR_BITOR          ] = "BITOR",
    [ IR_GET_UPVAL      ] = "GET_UPVAL",
    [ IR_GET_KEY        ] = "GET_KEY",
    [ IR_GET_INDEX      ] = "GET_INDEX",
    [ IR_SUPEROF        ] = "SUPEROF",
    [ IR_CALL           ] = "CALL",
    [ IR_ARRAY_APPEND   ] = "ARRAY_APPEND",
    [ IR_ARRAY_EXTEND   ] = "ARRAY_EXTEND",
};

static void debug_print_op( ir_function* f, unsigned i )
{
    const ir_op& op = f->ops.at( i );
    printf( ":%04X %s", i, OPCODE_NAMES[ op.opcode ] );
    for ( unsigned o = 0; o < op.ocount; ++o )
    {
        ir_operand operand = f->operands[ op.oindex + o ];

        if ( o )
        {
            printf( "," );
        }

        switch ( operand.kind )
        {
        case IR_O_NONE:
        {
            printf( " NONE" );
            break;
        }

        case IR_O_OP:
        {
            printf( " :%04X", operand.index );
            break;
        }

        case IR_O_JUMP:
        {
            printf( " @%04X", operand.index );
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

        case IR_O_AST_NUMBER:
        {
            const ast_node& n = f->ast->nodes.at( operand.index );
            printf( " %f", n.leaf_number().n );
            break;
        }

        case IR_O_AST_STRING:
        {
            const ast_node& n = f->ast->nodes.at( operand.index );
            printf( " \"%.*s\"", (int)n.leaf_string().size, n.leaf_string().text );
            break;
        }

        case IR_O_AST_KEY:
        {
            const ast_node& n = f->ast->nodes.at( operand.index );
            printf( " KEY '%.*s'", (int)n.leaf_string().size, n.leaf_string().text );
            break;
        }

        case IR_O_LOCAL_INDEX:
        {
            const ast_local& local = f->ast->locals.at( operand.index );
            printf( " LOCAL %.*s", (int)local.name.size(), local.name.data() );
            break;
        }

        case IR_O_UPVAL_INDEX:
        {
            printf( " UPVAL %u", operand.index );
            break;
        }

        case IR_O_FUNCTION:
        {
            printf( " FUNCTION %u", operand.index );
            break;
        }

        case IR_O_UPSTACK_INDEX:
        {
            printf( " UPSTACK INDEX %u", operand.index );
            break;
        }
        }
    }

    printf( "\n" );
}

void ir_function::debug_print()
{
    printf( "FUNCTION %s\n", ast->name.c_str() );
    for ( unsigned i = 0; i < ops.size(); ++i )
    {
        debug_print_op( this, i );
    }
}

}

