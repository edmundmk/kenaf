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
#include "../common/escape_string.h"

namespace kf
{

ir_function::ir_function()
    :   ast( nullptr )
{
}

ir_function::~ir_function()
{
}

#ifdef _MSC_VER
#define INIT( x )
#else
#define INIT( x ) [ x ] =
#endif

const char* const OPCODE_NAMES[] =
{
    INIT( IR_NOP            ) "NOP",

    INIT( IR_LENGTH         ) "LENGTH",
    INIT( IR_NEG            ) "NEG",
    INIT( IR_POS            ) "POS",
    INIT( IR_BITNOT         ) "BITNOT",
    INIT( IR_MUL            ) "MUL",
    INIT( IR_DIV            ) "DIV",
    INIT( IR_INTDIV         ) "INTDIV",
    INIT( IR_MOD            ) "MOD",
    INIT( IR_ADD            ) "ADD",
    INIT( IR_SUB            ) "SUB",
    INIT( IR_CONCAT         ) "CONCAT",
    INIT( IR_LSHIFT         ) "LSHIFT",
    INIT( IR_RSHIFT         ) "RSHIFT",
    INIT( IR_ASHIFT         ) "ASHIFT",
    INIT( IR_BITAND         ) "BITAND",
    INIT( IR_BITXOR         ) "BITXOR",
    INIT( IR_BITOR          ) "BITOR",

    INIT( IR_PARAM          ) "PARAM",
    INIT( IR_CONST          ) "CONST",
    INIT( IR_MOV            ) "MOV",

    INIT( IR_EQ             ) "EQ",
    INIT( IR_NE             ) "NE",
    INIT( IR_LT             ) "LT",
    INIT( IR_LE             ) "LE",
    INIT( IR_IS             ) "IS",
    INIT( IR_NOT            ) "NOT",

    INIT( IR_GET_GLOBAL     ) "GET_GLOBAL",
    INIT( IR_GET_KEY        ) "GET_KEY",
    INIT( IR_SET_KEY        ) "SET_KEY",
    INIT( IR_GET_INDEX      ) "GET_INDEX",
    INIT( IR_SET_INDEX      ) "SET_INDEX",
    INIT( IR_NEW_ENV        ) "NEW_ENV",
    INIT( IR_GET_ENV        ) "GET_ENV",
    INIT( IR_SET_ENV        ) "SET_ENV",
    INIT( IR_NEW_OBJECT     ) "NEW_OBJECT",
    INIT( IR_NEW_ARRAY      ) "NEW_ARRAY",
    INIT( IR_NEW_TABLE      ) "NEW_TABLE",
    INIT( IR_NEW_FUNCTION   ) "NEW_FUNCTION",
    INIT( IR_SUPER          ) "SUPER",
    INIT( IR_APPEND         ) "APPEND",

    INIT( IR_CALL           ) "CALL",
    INIT( IR_YCALL          ) "YCALL",
    INIT( IR_YIELD          ) "YIELD",
    INIT( IR_VARARG         ) "VARARG",
    INIT( IR_UNPACK         ) "UNPACK",
    INIT( IR_EXTEND         ) "EXTEND",

    INIT( IR_SELECT         ) "SELECT",

    INIT( IR_BLOCK          ) "BLOCK",
    INIT( IR_JUMP           ) "JUMP",
    INIT( IR_JUMP_TEST      ) "JUMP_TEST",
    INIT( IR_JUMP_THROW     ) "JUMP_THROW",
    INIT( IR_JUMP_RETURN    ) "JUMP_RETURN",
    INIT( IR_JUMP_FOR_EGEN  ) "JUMP_FOR_EGEN",
    INIT( IR_JUMP_FOR_EACH  ) "JUMP_FOR_EACH",
    INIT( IR_JUMP_FOR_SGEN  ) "JUMP_FOR_SGEN",
    INIT( IR_JUMP_FOR_STEP  ) "JUMP_FOR_STEP",

    INIT( IR_FOR_EACH_ITEMS ) "FOR_EACH_ITEMS",
    INIT( IR_FOR_STEP_INDEX ) "FOR_STEP_INDEX",

    INIT( IR_PHI            ) "PHI",
    INIT( IR_PHI_OPEN       ) "PHI_OPEN",
    INIT( IR_REF            ) "REF",

    INIT( IR_OP_INVALID     ) "INVALID",
};

const char* const BLOCK_KIND_NAMES[] =
{
    INIT( IR_BLOCK_NONE     ) "NONE",
    INIT( IR_BLOCK_BASIC    ) "BASIC",
    INIT( IR_BLOCK_LOOP     ) "LOOP",
    INIT( IR_BLOCK_UNSEALED ) "UNSEALED"
};

static void debug_print_op( const ir_function* f, unsigned i, int indent )
{
    const ir_op& op = f->ops[ i ];

    bool grey = op.opcode == IR_NOP || ( ( op.opcode == IR_PHI || op.opcode == IR_REF ) && indent == 0 );
    if ( grey )
    {
        return;
        printf( "\x1B[90m" );
    }

    printf( "%*s[%4u]:%04X", indent, "", op.sloc, i );
    if ( op.live_range != IR_INVALID_INDEX )
        printf( " ↓%04X", op.live_range );
    else
        printf( "      " );

    if ( op.mark )
        printf( " !" );
    else if ( op.r != IR_INVALID_REGISTER )
        printf( " r" );
    else
        printf( "  " );

    if ( op.r != IR_INVALID_REGISTER )
        printf( "%02u", op.r );
    else
        printf( "  " );

    if ( op.s != IR_INVALID_REGISTER )
        printf( "@%02u", op.s );
    else
        printf( "   " );

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

        case IR_O_TEMP:
        {
            printf( " TEMP :%04X", operand.index );
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
            const ir_constant& n = f->constants[ operand.index ];
            printf( " %f", n.n );
            break;
        }

        case IR_O_STRING:
        {
            const ir_constant& k = f->constants[ operand.index ];
            std::string s = escape_string( std::string_view( k.text, k.size ), 45 );
            printf( " %s", s.c_str() );
            break;
        }

        case IR_O_IMMEDIATE:
        {
            int i = (int8_t)operand.index;
            printf( " %i", i );
            break;
        }

        case IR_O_SELECTOR:
        {
            const ir_selector& s = f->selectors[ operand.index ];
            printf( " \'%.*s\'", (int)s.size, s.text );
            break;
        }

        case IR_O_LOCAL:
        {
            const ast_local& local = f->ast->locals[ operand.index ];
            printf( " LOCAL %.*s", (int)local.name.size(), local.name.data() );
            break;
        }

        case IR_O_OUTENV:
        {
            printf( " OUTENV %u", operand.index );
            break;
        }

        case IR_O_ENVSLOT:
        {
            printf( " ENV_SLOT %u", operand.index );
            break;
        }

        case IR_O_FUNCTION:
        {
            printf( " FUNCTION %u", operand.index );
            break;
        }

        case IR_O_IFUNCREF:
        {
            printf( " IFUNCREF %u", operand.index );
            break;
        }
        }
    }
    if ( op.local() != IR_INVALID_LOCAL )
    {
        std::string_view name = f->ast->locals[ op.local() ].name;
        printf( " /* %.*s */", (int)name.size(), name.data() );
    }

    if ( grey )
    {
        printf( "\x1B[0m" );
    }

    printf( "\n" );

    if ( op.opcode == IR_BLOCK )
    {
        const ir_block& block = f->blocks[ f->operands[ op.oindex ].index ];
        printf( "  %s :%04X:%04X", BLOCK_KIND_NAMES[ block.kind ], block.lower, block.upper );
        for ( unsigned preceding = block.preceding_lower; preceding < block.preceding_upper; ++preceding )
        {
            ir_block_index index = f->preceding_blocks[ preceding ];
            if ( index != IR_INVALID_INDEX )
                printf( " @%u", index );
        }
        printf( "\n" );
        for( unsigned phi = block.phi_head; phi != IR_INVALID_INDEX; phi = f->ops[ phi ].phi_next )
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

        for ( unsigned phi_index = block.phi_head; phi_index != IR_INVALID_INDEX; phi_index = ops[ phi_index ].phi_next )
        {
            const ir_op& phi = ops[ phi_index ];
            assert( phi.opcode == IR_PHI || phi.opcode == IR_REF );

            const ast_local& local = ast->locals[ phi.local() ];

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
                const ir_operand& operand = operands[ phi.oindex + j ];
                assert( operand.kind == IR_O_OP );

                const ir_op& to_op = ops[ operand.index ];
                const ast_local& to_local = ast->locals[ to_op.local() ];

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

