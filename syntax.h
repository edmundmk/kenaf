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
    AST_EXPR_NULL,
    AST_EXPR_FALSE,
    AST_EXPR_TRUE,
    AST_EXPR_NUMBER,
    AST_EXPR_STRING,
    AST_EXPR_IDENTIFIER,
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
    AST_OP_EQ,
    AST_OP_NE,
    AST_OP_LT,
    AST_OP_LE,
    AST_OP_GT,
    AST_OP_GE,
    AST_OP_IS,
    AST_OP_IS_NOT,
    AST_EXPR_NOT,
    AST_EXPR_AND,
    AST_EXPR_OR,
    AST_EXPR_COMPARE,
    AST_EXPR_LAMBDA,
    AST_EXPR_LAMBDA_GENERATOR,
    AST_EXPR_OBJECT_LITERAL,
    AST_EXPR_ARRAY_LITERAL,
    AST_EXPR_TABLE_LITERAL,
};

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
            size_t child_index;     // Index of first child, or this index if no children.
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

