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

    [ IR_PARAM          ] = "PARAM",
    [ IR_CONST          ] = "CONST",
    [ IR_VAL            ] = "VAL",
    [ IR_PIN            ] = "PIN",

    [ IR_EQ             ] = "EQ",
    [ IR_NE             ] = "NE",
    [ IR_LT             ] = "LT",
    [ IR_LE             ] = "LE",
    [ IR_IS             ] = "IS",
    [ IR_NOT            ] = "NOT",

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
    [ IR_YCALL          ] = "YCALL",
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

    [ IR_BLOCK          ] = "BLOCK",
    [ IR_JUMP           ] = "JUMP",
    [ IR_JUMP_TEST      ] = "JUMP_TEST",
    [ IR_JUMP_TFOR      ] = "JUMP_TFOR",
    [ IR_JUMP_THROW     ] = "JUMP_THROW",
    [ IR_JUMP_RETURN    ] = "JUMP_RETURN",

    [ IR_PHI            ] = "PHI",
    [ IR_PHI_OPEN       ] = "PHI_OPEN",
};

const char* const BLOCK_KIND_NAMES[] =
{
    [ IR_BLOCK_BASIC    ] = "BASIC",
    [ IR_BLOCK_LOOP     ] = "LOOP",
    [ IR_BLOCK_UNSEALED ] = "UNSEALED"
};

static void debug_print_op( const ir_function* f, unsigned i, int indent )
{
    const ir_op& op = f->ops.at( i );
    printf( "%*s:%04X %s", indent, "", i, OPCODE_NAMES[ op.opcode ] );
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
        case IR_O_PIN:
        {
            printf( " :%04X", operand.index );
            break;
        }

        case IR_O_SELECT:
        {
            printf( " SELECT %u", operand.index );
            break;
        }

        case IR_O_BLOCK:
        {
            printf( " @%u", operand.index );
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

    if ( op.opcode == IR_BLOCK )
    {
        const ir_block& block = f->blocks.at( f->operands.at( op.oindex ).index );
        printf( "  %s", BLOCK_KIND_NAMES[ block.kind ] );
        if ( block.loop != IR_INVALID_INDEX )
            printf( " / LOOP @%u", block.loop );
        if ( block.preceding_lower < block.preceding_upper )
        {
            printf( " / PRECEDING" );
            for ( unsigned preceding = block.preceding_lower; preceding < block.preceding_upper; ++preceding )
            {
                ir_block_index index = f->preceding_blocks.at( preceding );
                if ( index != IR_INVALID_INDEX )
                    printf( " @%u", index );
            }
        }
        printf( "\n" );
        for( unsigned phi = block.phi_head; phi != IR_INVALID_INDEX; phi = f->ops.at( phi ).phi_next )
        {
            debug_print_op( f, phi, 2 );
        }
        printf( "  OPS :%04X:%04X\n", block.lower, block.upper );
    }
}

void ir_function::debug_print() const
{
    printf( "FUNCTION %s\n", ast->name.c_str() );
    for ( unsigned i = 0; i < ops.size(); ++i )
    {
        debug_print_op( this, i, 0 );
    }
}

}

