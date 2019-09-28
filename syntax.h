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
    AST_LEAF_NODE,
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
};

struct syntax_node
{
    uint16_t kind;          // AST node kind.
    uint16_t leaf;           // Leaf or non leaf?
    srcloc sloc;            // Source location.
    union
    {
        // Non-leaf node.
        struct
        {
            size_t child_index;     // Index of first child, or a custom index.
            size_t next_index;      // Index of next sibling, fixed up afterwards.
        };
        // Leaf node with string value.
        struct { const char* text; size_t size; } s;
        // Leaf node with number value.
        double n;
        // Leaf node with operator.
        unsigned op;
    };
};

}

#endif

