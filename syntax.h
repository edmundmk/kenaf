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
    std::vector< std::unique_ptr< syntax_function > > functions;
};

struct syntax_function
{
    std::vector< syntax_node > nodes;
};

enum syntax_node_leaf
{
    AST_NON_LEAF,
    AST_LEAF_STRING,
    AST_LEAF_NUMBER,
};

enum syntax_node_kind
{
};

struct syntax_node
{
    uint32_t kind : 30;     // AST node kind.
    uint32_t leaf : 2;      // Leaf or non leaf?
    srcloc sloc;            // Source location.
    union
    {
        struct
        {
            size_t child_index;     // Index of first child, or a custom index.
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

