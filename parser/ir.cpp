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

    [ IR_EQ             ] = "EQ",
    [ IR_NE             ] = "NE",
    [ IR_LT             ] = "LT",
    [ IR_LE             ] = "LE",
    [ IR_IS             ] = "IS",
    [ IR_NOT            ] = "NOT",

    [ IR_L              ] = "L",
    [ IR_LOAD           ] = "LOAD",

    [ IR_GET_GLOBAL     ] = "GET_GLOBAL",
    [ IR_GET_UPVAL      ] = "GET_UPVAL",
    [ IR_SET_UPVAL      ] = "SET_UPVAL",
    [ IR_GET_KEY        ] = "GET_KEY",
    [ IR_SET_KEY        ] = "SET_KEY",
    [ IR_GET_INDEX      ] = "GET_INDEX",
    [ IR_SET_INDEX      ] = "SET_INDEX",
    [ IR_SUPEROF        ] = "SUPEROF",
    [ IR_APPEND         ] = "APPEND",
    [ IR_NEW_ARRAY      ] = "NEW_ARRAY",
    [ IR_NEW_TABLE      ] = "NEW_TABLE",

    [ IR_CALL           ] = "CALL",
    [ IR_YIELD_FOR      ] = "YIELD_FOR",
    [ IR_YIELD          ] = "YIELD",
    [ IR_VARARG         ] = "VARARG",
    [ IR_UNPACK         ] = "UNPACK",
    [ IR_EXTEND         ] = "EXTEND",

    [ IR_SELECT         ] = "SELECT",

    [ IR_CLOSE_UPSTACK  ] = "CLOSE_UPSTACK",

    [ IR_FOR_EACH_HEAD  ] = "FOR_EACH_HEAD",
    [ IR_FOR_EACH       ] = "FOR_EACH",
    [ IR_FOR_STEP_HEAD  ] = "FOR_STEP_HEAD",
    [ IR_FOR_STEP       ] = "FOR_STEP",

    [ IR_B_AND          ] = "B_AND",
    [ IR_B_CUT          ] = "B_CUT",
    [ IR_B_DEF          ] = "B_DEF",
    [ IR_B_PHI          ] = "B_PHI",

    [ IR_BLOCK_HEAD     ] = "BLOCK_HEAD",
    [ IR_BLOCK_JUMP     ] = "BLOCK_JUMP",
    [ IR_BLOCK_TEST     ] = "BLOCK_TEST",
    [ IR_BLOCK_SHORTCUT ] = "BLOCK_SHORTCUT",
    [ IR_BLOCK_FOR_TEST ] = "BLOCK_FOR_TEST",
    [ IR_BLOCK_RETURN   ] = "BLOCK_RETURN",
    [ IR_BLOCK_THROW    ] = "BLOCK_THROW",
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

        case IR_O_NUMBER:
        {
            const ir_number& n = f->numbers.at( operand.index );
            printf( " %f", n.n );
            break;
        }

        case IR_O_STRING:
        {
            const ir_string& s = f->strings.at( operand.index );
            printf( " \"%.*s\"", (int)s.size, s.text );
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

        case IR_O_FUNCTION_INDEX:
        {
            printf( " FUNCTION %u", operand.index );
            break;
        }

        case IR_O_UPSTACK_INDEX:
        {
            printf( " UPSTACK %u", operand.index );
            break;
        }
        }
    }
    if ( op.local != IR_INVALID_LOCAL )
    {
        std::string_view name = f->ast->locals.at( op.local ).name;
        printf( " /* %.*s */", (int)name.size(), name.data() );
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

