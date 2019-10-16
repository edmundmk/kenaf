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
    [ IR_MOV            ] = "MOV",

    [ IR_EQ             ] = "EQ",
    [ IR_NE             ] = "NE",
    [ IR_LT             ] = "LT",
    [ IR_LE             ] = "LE",
    [ IR_IS             ] = "IS",
    [ IR_NOT            ] = "NOT",

    [ IR_GET_GLOBAL     ] = "GET_GLOBAL",
    [ IR_GET_KEY        ] = "GET_KEY",
    [ IR_SET_KEY        ] = "SET_KEY",
    [ IR_GET_INDEX      ] = "GET_INDEX",
    [ IR_SET_INDEX      ] = "SET_INDEX",
    [ IR_NEW_ENV        ] = "NEW_ENV",
    [ IR_GET_ENV        ] = "GET_ENV",
    [ IR_SET_ENV        ] = "SET_ENV",
    [ IR_NEW_OBJECT     ] = "NEW_OBJECT",
    [ IR_NEW_ARRAY      ] = "NEW_ARRAY",
    [ IR_NEW_TABLE      ] = "NEW_TABLE",
    [ IR_NEW_FUNCTION   ] = "NEW_FUNCTION",
    [ IR_SUPER          ] = "SUPER",
    [ IR_APPEND         ] = "APPEND",

    [ IR_CALL           ] = "CALL",
    [ IR_YCALL          ] = "YCALL",
    [ IR_YIELD          ] = "YIELD",
    [ IR_VARARG         ] = "VARARG",
    [ IR_UNPACK         ] = "UNPACK",
    [ IR_EXTEND         ] = "EXTEND",

    [ IR_SELECT         ] = "SELECT",

    [ IR_B_AND          ] = "B_AND",
    [ IR_B_CUT          ] = "B_CUT",
    [ IR_B_DEF          ] = "B_DEF",
    [ IR_B_PHI          ] = "B_PHI",

    [ IR_BLOCK          ] = "BLOCK",
    [ IR_JUMP           ] = "JUMP",
    [ IR_JUMP_TEST      ] = "JUMP_TEST",
    [ IR_JUMP_THROW     ] = "JUMP_THROW",
    [ IR_JUMP_RETURN    ] = "JUMP_RETURN",
    [ IR_JUMP_FOR_EGEN  ] = "JUMP_FOR_EGEN",
    [ IR_JUMP_FOR_EACH  ] = "JUMP_FOR_EACH",
    [ IR_JUMP_FOR_SGEN  ] = "JUMP_FOR_SGEN",
    [ IR_JUMP_FOR_STEP  ] = "JUMP_FOR_STEP",

    [ IR_FOR_EACH_ITEMS ] = "FOR_EACH_ITEMS",
    [ IR_FOR_STEP_INDEX ] = "FOR_STEP_INDEX",

    [ IR_PHI            ] = "PHI",
    [ IR_PHI_OPEN       ] = "PHI_OPEN",
    [ IR_REF            ] = "REF",
};

const char* const BLOCK_KIND_NAMES[] =
{
    [ IR_BLOCK_NONE     ] = "NONE",
    [ IR_BLOCK_BASIC    ] = "BASIC",
    [ IR_BLOCK_LOOP     ] = "LOOP",
    [ IR_BLOCK_UNSEALED ] = "UNSEALED"
};

static void debug_print_op( const ir_function* f, unsigned i, int indent )
{
    const ir_op& op = f->ops.at( i );

    bool grey = op.opcode == IR_NOP || ( ( op.opcode == IR_PHI || op.opcode == IR_REF ) && indent == 0 );
    if ( grey )
    {
        return;
        printf( "\x1B[90m" );
    }

    printf( "%*s:%04X", indent, "", i );
    if ( op.live_range != IR_INVALID_INDEX )
        printf( " ↓%04X", op.live_range );
    else if ( op.mark )
        printf( " ↓====" );
    else
        printf( "      " );
    printf( " %s", OPCODE_NAMES[ op.opcode ] );

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
            const ir_constant& n = f->constants.at( operand.index );
            printf( " %f", n.n );
            break;
        }

        case IR_O_STRING:
        {
            const ir_constant& s = f->constants.at( operand.index );
            printf( " \"%.*s\"", (int)s.size, s.text );
            break;
        }

        case IR_O_SELECTOR:
        {
            const ir_selector& s = f->selectors.at( operand.index );
            printf( " \'%.*s\'", (int)s.size, s.text );
            break;
        }

        case IR_O_IMMEDIATE:
        {
            int i = (int8_t)operand.index;
            printf( " %i", i );
            break;
        }

        case IR_O_LOCAL_INDEX:
        {
            const ast_local& local = f->ast->locals.at( operand.index );
            printf( " LOCAL %.*s", (int)local.name.size(), local.name.data() );
            break;
        }

        case IR_O_OUTENV_INDEX:
        {
            printf( " OUTENV %u", operand.index );
            break;
        }

        case IR_O_ENV_SLOT_INDEX:
        {
            printf( " ENV_SLOT %u", operand.index );
            break;
        }

        case IR_O_FUNCTION_INDEX:
        {
            printf( " FUNCTION %u", operand.index );
            break;
        }
        }
    }
    if ( op.local() != IR_INVALID_LOCAL )
    {
        std::string_view name = f->ast->locals.at( op.local() ).name;
        printf( " /* %.*s */", (int)name.size(), name.data() );
    }

    if ( grey )
    {
        printf( "\x1B[0m" );
    }

    printf( "\n" );

    if ( op.opcode == IR_BLOCK )
    {
        const ir_block& block = f->blocks.at( f->operands.at( op.oindex ).index );
        printf( "  %s :%04X:%04X", BLOCK_KIND_NAMES[ block.kind ], block.lower, block.upper );
        for ( unsigned preceding = block.preceding_lower; preceding < block.preceding_upper; ++preceding )
        {
            ir_block_index index = f->preceding_blocks.at( preceding );
            if ( index != IR_INVALID_INDEX )
                printf( " @%u", index );
        }
        printf( "\n" );
        for( unsigned phi = block.phi_head; phi != IR_INVALID_INDEX; phi = f->ops.at( phi ).phi_next )
        {
            debug_print_op( f, phi, 2 );
        }
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

void ir_function::debug_print_phi_graph() const
{
    printf( "digraph { rankdir = BT;\n" );
    for ( unsigned block_index = 0; block_index < blocks.size(); ++block_index )
    {
        const ir_block& block = blocks[ block_index ];

        for ( unsigned phi_index = block.phi_head; phi_index != IR_INVALID_INDEX; phi_index = ops.at( phi_index ).phi_next )
        {
            const ir_op& phi = ops.at( phi_index );
            assert( phi.opcode == IR_PHI || phi.opcode == IR_REF );

            const ast_local& local = ast->locals.at( phi.local() );

            if ( phi.opcode == IR_REF || block.kind == IR_BLOCK_LOOP )
            {
                printf
                (
                    "%.*s_%04X [style=filled, fillcolor=%s];\n",
                    (int)local.name.size(), local.name.data(),
                    phi_index,
                    phi.opcode == IR_REF ? "grey" : "lightsteelblue"
                );
            }

            for ( unsigned j = 0; j < phi.ocount; ++j )
            {
                const ir_operand& operand = operands.at( phi.oindex + j );
                assert( operand.kind == IR_O_OP );

                const ir_op& to_op = ops.at( operand.index );
                const ast_local& to_local = ast->locals.at( to_op.local() );

                printf
                (
                    "%.*s_%04X -> %.*s_%04X;\n",
                    (int)local.name.size(), local.name.data(),
                    phi_index,
                    (int)to_local.name.size(), to_local.name.data(),
                    operand.index
                );
            }
        }
    }
    printf( "}\n" );
}

}

