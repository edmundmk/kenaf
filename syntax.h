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

#include <assert.h>
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
struct syntax_leaf_string;
struct syntax_leaf_number;
struct syntax_leaf_function;

struct syntax_tree
{
    syntax_tree();
    ~syntax_tree();

    syntax_function* new_function( srcloc sloc, syntax_function* outer );

    std::vector< std::unique_ptr< syntax_function > > functions;
};

struct syntax_function
{
    syntax_function( srcloc sloc, syntax_function* outer );
    ~syntax_function();

    void fixup_nodes();
    void debug_print();

    srcloc sloc;
    std::string name;
    syntax_function* outer;
    std::vector< syntax_node > nodes;
    bool implicit_self;
    bool is_generator;
    bool is_top_level;

};

enum syntax_node_kind : uint16_t
{
    AST_FUNCTION,           // parameters block

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

    AST_EXPR_ARRAY,
    AST_EXPR_TABLE,
    AST_KEYVAL,

    AST_OP_EQ,
    AST_OP_NE,
    AST_OP_LT,
    AST_OP_LE,
    AST_OP_GT,
    AST_OP_GE,
    AST_OP_IS,
    AST_OP_IS_NOT,

    AST_DEFINITION,         // name|qual_name def
    AST_DEF_FUNCTION,       // leaf function
    AST_DEF_OBJECT,         // prototype? object_key|definition*

    AST_PARAMETERS,         // name* vararg_param?
    AST_VARARG_PARAM,       // name

    AST_PROTOTYPE,          // expr
    AST_OBJECT_KEY,         // name expr
};

enum syntax_node_leaf : uint8_t
{
    AST_NON_LEAF,
    AST_LEAF_STRING,
    AST_LEAF_NUMBER,
    AST_LEAF_FUNCTION,
};

const size_t AST_INVALID_INDEX = ~(size_t)0;

struct syntax_node
{
    syntax_node_kind kind;  // AST node kind.
    syntax_node_leaf leaf;  // Leaf or non leaf?
    uint8_t prev_leaf;      // Is the previous node a leaf?.
    srcloc sloc;            // Source location.
    unsigned child_index;   // Index of first child, or invalid.
    unsigned next_index;    // Index of next sibling, fixed up afterwards.

    const syntax_leaf_string& leaf_string() const;
    const syntax_leaf_number& leaf_number() const;
    const syntax_leaf_function& leaf_function() const;

    syntax_leaf_string& leaf_string();
    syntax_leaf_number& leaf_number();
    syntax_leaf_function& leaf_function();
};

struct syntax_leaf_string
{
    const char* text;
    size_t size;
};

struct syntax_leaf_number
{
    double n;
};

struct syntax_leaf_function
{
    syntax_function* function;
};

inline const syntax_leaf_string& syntax_node::leaf_string() const
{
    static_assert( sizeof( syntax_leaf_string ) <= sizeof( syntax_node ) );
    assert( leaf == AST_LEAF_STRING );
    return *(const syntax_leaf_string*)( this + 1 );
}

inline const syntax_leaf_number& syntax_node::leaf_number() const
{
    static_assert( sizeof( syntax_leaf_number ) <= sizeof( syntax_node ) );
    assert( leaf == AST_LEAF_NUMBER );
    return *(const syntax_leaf_number*)( this + 1 );
}

inline const syntax_leaf_function& syntax_node::leaf_function() const
{
    static_assert( sizeof( syntax_leaf_function ) <= sizeof( syntax_node ) );
    assert( leaf == AST_LEAF_FUNCTION );
    return *(const syntax_leaf_function*)( this + 1 );
}

inline syntax_leaf_string& syntax_node::leaf_string()
{
    static_assert( sizeof( syntax_leaf_string ) <= sizeof( syntax_node ) );
    assert( leaf == AST_LEAF_STRING );
    return *(syntax_leaf_string*)( this + 1 );
}

inline syntax_leaf_number& syntax_node::leaf_number()
{
    static_assert( sizeof( syntax_leaf_number ) <= sizeof( syntax_node ) );
    assert( leaf == AST_LEAF_NUMBER );
    return *(syntax_leaf_number*)( this + 1 );
}

inline syntax_leaf_function& syntax_node::leaf_function()
{
    static_assert( sizeof( syntax_leaf_function ) <= sizeof( syntax_node ) );
    assert( leaf == AST_LEAF_FUNCTION );
    return *(syntax_leaf_function*)( this + 1 );
}

}

#endif

