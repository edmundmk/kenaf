//
//  syntax.h
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef SYNTAX_H
#define SYNTAX_H

#include <vector>
#include "source.h"

namespace kf
{

/*
    The parser builds an AST for each function.  The AST is stored in a linear
    fashion, with parent nodes occuring after child nodes:

                nibling
            previous sibling
                child
                child
            node
                nephew
                neice
            next sibling
        parent
*/

struct syntax_tree;
struct syntax_function;
struct syntax_node;

struct syntax_tree
{
    syntax_tree();
    ~syntax_tree();

    syntax_function* new_function();

    std::vector< std::unique_ptr< syntax_function > > functions;
};

struct syntax_function
{
    syntax_function();
    ~syntax_function();

    std::vector< syntax_node > nodes;
};

enum syntax_node_leaf
{
    AST_NON_LEAF,
    AST_LEAF_STRING,
    AST_LEAF_NUMBER,
    AST_LEAF_OPERATOR,
};

enum syntax_node_kind
{
    AST_BLOCK,              // stmt|call-expr|yield-expr*

    AST_STMT_VAR,           // name|name_list rval|rval_list?
    AST_STMT_IF,            // expr block elif* block?
    AST_STMT_FOR_STEP,      // name expr expr expr block
    AST_STMT_FOR_EACH,      // name|name_list expr block
    AST_STMT_WHILE,         // expr block
    AST_STMT_REPEAT,        // block expr
    AST_STMT_BREAK,
    AST_STMT_CONTINUE,
    AST_STMT_RETURN,        // expr*
    AST_STMT_THROW,         // expr

    AST_NAME_LIST,          // name+
    AST_ELIF,               // expr block

    AST_ASSIGN,             // lval|lval_list rval|rval_list
    AST_OP_ASSIGN,          // lval [AST_OP_MUL] expr
    AST_LVAL_LIST,          // expr+
    AST_RVAL_LIST,          // expr+

    AST_EXPR_YIELD,         // expr+
    AST_EXPR_YIELD_FOR,     // expr

    AST_EXPR_NULL,
    AST_EXPR_FALSE,
    AST_EXPR_TRUE,
    AST_EXPR_NUMBER,
    AST_EXPR_STRING,
    AST_EXPR_NAME,
    AST_EXPR_KEY,
    AST_EXPR_INDEX,
    AST_EXPR_CALL,
    AST_EXPR_LENGTH,
    AST_EXPR_NEG,
    AST_EXPR_POS,
    AST_EXPR_BITNOT,
    AST_EXPR_MUL,
    AST_EXPR_DIV,
    AST_EXPR_INTDIV,
    AST_EXPR_MOD,
    AST_EXPR_ADD,
    AST_EXPR_SUB,
    AST_EXPR_CONCAT,
    AST_EXPR_LSHIFT,
    AST_EXPR_RSHIFT,
    AST_EXPR_ASHIFT,
    AST_EXPR_BITAND,
    AST_EXPR_BITXOR,
    AST_EXPR_BITOR,
    AST_EXPR_COMPARE,
    AST_EXPR_NOT,
    AST_EXPR_AND,
    AST_EXPR_OR,
    AST_EXPR_IF,            // expr expr expr_elif* expr
    AST_EXPR_ELIF,          // expr expr
    AST_EXPR_UNPACK,        // last expression in list with ...

    AST_OP_EQ,
    AST_OP_NE,
    AST_OP_LT,
    AST_OP_LE,
    AST_OP_GT,
    AST_OP_GE,
    AST_OP_IS,
    AST_OP_IS_NOT,

    AST_CONSTRUCT_LAMBDA,
    AST_CONSTRUCT_LAMBDA_GENERATOR,
    AST_CONSTRUCT_OBJECT,
    AST_CONSTRUCT_ARRAY,
    AST_CONSTRUCT_TABLE,
};

const size_t AST_INVALID_INDEX = ~(size_t)0;

struct syntax_node
{
    uint16_t kind;          // AST node kind.
    uint16_t leaf;          // Leaf or non leaf?
    srcloc sloc;            // Source location.
    union
    {
        // Non-leaf node.
        struct
        {
            size_t child_index;     // Index of first child, or invalid.
            size_t next_index;      // Index of next sibling, fixed up afterwards.
        };
        // Leaf node with string value.
        struct { const char* text; size_t size; } s;
        // Leaf node with number value.
        double n;
    };
};

}

#endif

