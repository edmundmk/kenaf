//
//  ast.cpp
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ast.h"

namespace kf
{

const char* const AST_NODE_NAME[] =
{
    [ AST_NONE              ] = "NONE",

    [ AST_EXPR_LENGTH       ] = "EXPR_LENGTH",
    [ AST_EXPR_NEG          ] = "EXPR_NEG",
    [ AST_EXPR_POS          ] = "EXPR_POS",
    [ AST_EXPR_BITNOT       ] = "EXPR_BITNOT",
    [ AST_EXPR_MUL          ] = "EXPR_MUL",
    [ AST_EXPR_DIV          ] = "EXPR_DIV",
    [ AST_EXPR_INTDIV       ] = "EXPR_INTDIV",
    [ AST_EXPR_MOD          ] = "EXPR_MOD",
    [ AST_EXPR_ADD          ] = "EXPR_ADD",
    [ AST_EXPR_SUB          ] = "EXPR_SUB",
    [ AST_EXPR_CONCAT       ] = "EXPR_CONCAT",
    [ AST_EXPR_LSHIFT       ] = "EXPR_LSHIFT",
    [ AST_EXPR_RSHIFT       ] = "EXPR_RSHIFT",
    [ AST_EXPR_ASHIFT       ] = "EXPR_ASHIFT",
    [ AST_EXPR_BITAND       ] = "EXPR_BITAND",
    [ AST_EXPR_BITXOR       ] = "EXPR_BITXOR",
    [ AST_EXPR_BITOR        ] = "EXPR_BITOR",

    [ AST_EXPR_COMPARE      ] = "EXPR_COMPARE",
    [ AST_OP_EQ             ] = "OP_EQ",
    [ AST_OP_NE             ] = "OP_NE",
    [ AST_OP_LT             ] = "OP_LT",
    [ AST_OP_LE             ] = "OP_LE",
    [ AST_OP_GT             ] = "OP_GT",
    [ AST_OP_GE             ] = "OP_GE",
    [ AST_OP_IS             ] = "OP_IS",
    [ AST_OP_IS_NOT         ] = "OP_IS_NOT",

    [ AST_EXPR_NULL         ] = "EXPR_NULL",
    [ AST_EXPR_FALSE        ] = "EXPR_FALSE",
    [ AST_EXPR_TRUE         ] = "EXPR_TRUE",
    [ AST_EXPR_NUMBER       ] = "EXPR_NUMBER",
    [ AST_EXPR_STRING       ] = "EXPR_STRING",

    [ AST_EXPR_NOT          ] = "EXPR_NOT",
    [ AST_EXPR_AND          ] = "EXPR_AND",
    [ AST_EXPR_OR           ] = "EXPR_OR",
    [ AST_EXPR_IF           ] = "EXPR_IF",
    [ AST_EXPR_ELIF         ] = "EXPR_ELIF",

    [ AST_EXPR_KEY          ] = "EXPR_KEY",
    [ AST_EXPR_INDEX        ] = "EXPR_INDEX",
    [ AST_EXPR_CALL         ] = "EXPR_CALL",
    [ AST_EXPR_UNPACK       ] = "EXPR_UNPACK",
    [ AST_EXPR_ARRAY        ] = "EXPR_ARRAY",
    [ AST_EXPR_TABLE        ] = "EXPR_TABLE",
    [ AST_TABLE_KEY         ] = "TABLE_KEY",
    [ AST_EXPR_YIELD        ] = "EXPR_YIELD",
    [ AST_EXPR_YIELD_FOR    ] = "EXPR_YIELD_FOR",

    [ AST_FUNCTION          ] = "FUNCTION",
    [ AST_PARAMETERS        ] = "PARAMETERS",
    [ AST_VARARG_PARAM      ] = "VARARG_PARAM",

    [ AST_BLOCK             ] = "BLOCK",

    [ AST_STMT_VAR          ] = "STMT_VAR",
    [ AST_DEFINITION        ] = "DEFINITION",
    [ AST_ASSIGN            ] = "ASSIGN",
    [ AST_OP_ASSIGN         ] = "OP_ASSIGN",
    [ AST_NAME_LIST         ] = "NAME_LIST",
    [ AST_LVAL_LIST         ] = "LVAL_LIST",
    [ AST_RVAL_LIST         ] = "RVAL_LIST",

    [ AST_STMT_IF           ] = "STMT_IF",
    [ AST_STMT_ELIF         ] = "STMT_ELIF",
    [ AST_STMT_FOR_STEP     ] = "STMT_FOR_STEP",
    [ AST_STMT_FOR_EACH     ] = "STMT_FOR_EACH",
    [ AST_STMT_WHILE        ] = "STMT_WHILE",
    [ AST_STMT_REPEAT       ] = "STMT_REPEAT",
    [ AST_STMT_BREAK        ] = "STMT_BREAK",
    [ AST_STMT_CONTINUE     ] = "STMT_CONTINUE",
    [ AST_STMT_RETURN       ] = "STMT_RETURN",
    [ AST_STMT_THROW        ] = "STMT_THROW",

    [ AST_DEF_FUNCTION      ] = "DEF_FUNCTION",
    [ AST_DEF_OBJECT        ] = "DEF_OBJECT",
    [ AST_OBJECT_PROTOTYPE  ] = "OBJECT_PROTOTYPE",
    [ AST_OBJECT_KEY        ] = "OBJECT_KEY",

    [ AST_NAME              ] = "EXPR_NAME",
    [ AST_GLOBAL_NAME       ] = "GLOBAL_NAME",
    [ AST_UPVAL_NAME        ] = "UPVAL_NAME",
    [ AST_LOCAL_DECL        ] = "LOCAL_DECL",
    [ AST_LOCAL_NAME        ] = "LOCAL_NAME",
    [ AST_UPVAL_NAME_SUPER  ] = "UPVAL_NAME_SUPER",
    [ AST_LOCAL_NAME_SUPER  ] = "LOCAL_NAME_SUPER",
};

ast_script::ast_script()
{
}

ast_script::~ast_script()
{
}

ast_function* ast_script::new_function( srcloc sloc, ast_function* outer )
{
    functions.push_back( std::make_unique< ast_function >( sloc, outer ) );
    return functions.back().get();
}

ast_function::ast_function( srcloc sloc, ast_function* outer )
    :   sloc( sloc )
    ,   outer( outer )
    ,   parameter_count( 0 )
    ,   max_upstack_size( 0 )
    ,   implicit_self( false )
    ,   is_generator( false )
    ,   is_top_level( false )
    ,   is_varargs( false )
{
}

ast_function::~ast_function()
{
}

void ast_function::fixup_nodes()
{
    // Calculate next sibling pointers.
    unsigned last_index = 0;
    for ( unsigned index = 0; index < nodes.size(); ++index )
    {
        if ( index )
        {
            // Link last child node to its parent.
            nodes[ last_index ].next_index = index;

            // Remember if last index so we can move backwards in vector.
            if ( nodes[ last_index ].leaf )
            {
                nodes[ index ].prev_leaf = true;
            }
        }
        last_index = index;

        // Find oldest descendant.
        unsigned node_index = index;
        unsigned child_index = nodes[ node_index ].child_index;
        while ( child_index != node_index )
        {
            node_index = child_index;
            child_index = nodes[ node_index ].child_index;
        }

        // Previous node is the node before the oldest descendant.
        if ( child_index )
        {
            unsigned prev_index = child_index - 1;
            if ( nodes[ child_index ].prev_leaf )
            {
                prev_index -= 1;
            }
            nodes[ prev_index ].next_index = index;
        }

        // Skip leaf data.
        if ( nodes[ index ].leaf )
        {
            ++index;
        }
    }
}

static void debug_print_tree( const std::vector< ast_node >& nodes, unsigned index, int indent )
{
    const ast_node& n = nodes.at( index );

    printf( "%*s%s", indent, "", AST_NODE_NAME[ n.kind ] );
    if ( n.leaf == AST_LEAF_STRING )
    {
        const ast_leaf_string& l = n.leaf_string();
        printf( " STRING '%.*s'\n", (int)l.size, l.text );
    }
    else if ( n.leaf == AST_LEAF_NUMBER )
    {
        const ast_leaf_number& l = n.leaf_number();
        printf( " NUMBER %f\n", l.n );
    }
    else if ( n.leaf == AST_LEAF_FUNCTION )
    {
        const ast_leaf_function& l = n.leaf_function();
        printf( " FUNCTION %p %s\n", l.function, l.function->name.c_str() );
    }
    else if ( n.leaf == AST_LEAF_INDEX )
    {
        const ast_leaf_index& l = n.leaf_index();
        if ( l.index != AST_INVALID_INDEX )
            printf( " INDEX %u\n", l.index );
        else
            printf( " INVALID INDEX\n" );
    }
    else
    {
        printf( "\n" );
    }

    for ( unsigned c = n.child_index; c < index; c = nodes[ c ].next_index )
    {
        debug_print_tree( nodes, c, indent + 2 );
    }
}

void ast_function::debug_print()
{
    printf( "FUNCTION %p %s\n", this, name.c_str() );
    if ( outer )
        printf( "  OUTER %p %s\n", outer, outer->name.c_str() );
    printf( "  %u PARAMETERS\n", parameter_count );
    printf( "  %u MAX_UPSTACK_SIZE\n", max_upstack_size );
    if ( implicit_self )
        printf( "  IMPLICIT_SELF\n" );
    if ( is_generator )
        printf( "  GENERATOR\n" );
    if ( is_top_level )
        printf( "  TOP_LEVEL\n" );
    if ( is_varargs )
        printf( "  VARARGS\n" );

    printf( "  UPVALS:\n" );
    for ( size_t i = 0; i < upvals.size(); ++i )
    {
        const ast_upval& upval = upvals[ i ];
        printf
        (
            "    %zu : %s %u\n", i,
            upval.outer_upval ? "OUTER_UPVAL" : "OUTER_LOCAL",
            upval.outer_index
        );
    }

    printf( "  LOCALS:\n" );
    for ( size_t i = 0; i < locals.size(); ++i )
    {
        const ast_local& local = locals[ i ];
        printf( "    %zu : %.*s", i, (int)local.name.size(), local.name.data() );
        if ( local.upstack_index != AST_INVALID_INDEX )
            printf( " UPSTACK %u", local.upstack_index );
        if ( local.is_implicit_self )
            printf( " IMPLICIT_SELF" );
        if ( local.is_parameter )
            printf( " PARAMETER" );
        if ( local.is_vararg_param )
            printf( " VARARG_PARAM" );
        printf( "\n" );
    }

    debug_print_tree( nodes, nodes.size() - 1, 2 );
}

}

