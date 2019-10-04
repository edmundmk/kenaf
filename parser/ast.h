//
//  ast.h
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef AST_H
#define AST_H

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

#include <assert.h>
#include <vector>
#include "source.h"

namespace kf
{

struct ast_script;
struct ast_function;
struct ast_node;
struct ast_leaf_string;
struct ast_leaf_number;
struct ast_leaf_function;
struct ast_leaf_index;

const unsigned AST_INVALID_INDEX = ~(unsigned)0;

struct ast_script
{
    ast_script();
    ~ast_script();

    ast_function* new_function( srcloc sloc, ast_function* outer );

    std::vector< std::unique_ptr< ast_function > > functions;
};

struct ast_upval
{
    unsigned outer_index;       // Index in outer function's upvals or locals.
    bool outer_upval;           // If true, upval was an upval for the outer function.
};

struct ast_local
{
    std::string_view name;      // Name of local or parameter.
    unsigned upstack_index;     // Index in upval stack, or AST_INVALID_INDEX.
    bool is_implicit_self;      // Is it implicit self?
    bool is_parameter;          // Is it a parameter?
    bool is_vararg_param;       // Is it the variable argument parameter?
};

struct ast_function
{
    ast_function( srcloc sloc, ast_function* outer );
    ~ast_function();

    void fixup_nodes();
    void debug_print();

    srcloc sloc;                // Source location of function.
    std::string name;           // Name of function.
    ast_function* outer;        // Lexically outer function.
    unsigned parameter_count;   // First count locals are parameters.
    unsigned max_upstack_size;  // Maximum size of upstack.
    bool implicit_self;         // Does the function have implicit self?
    bool is_generator;          // Is it a generator?
    bool is_top_level;          // Is it the top-level function of a script?
    bool is_varargs;            // Does it have a varargs parameter.

    std::vector< ast_upval > upvals;
    std::vector< ast_local > locals;
    std::vector< ast_node > nodes;
};

enum ast_node_kind : uint16_t
{
    AST_NONE,

    AST_EXPR_LENGTH,            // expr
    AST_EXPR_NEG,               // expr
    AST_EXPR_POS,               // expr
    AST_EXPR_BITNOT,            // expr
    AST_EXPR_MUL,               // expr expr
    AST_EXPR_DIV,               // expr expr
    AST_EXPR_INTDIV,            // expr expr
    AST_EXPR_MOD,               // expr expr
    AST_EXPR_ADD,               // expr expr
    AST_EXPR_SUB,               // expr expr
    AST_EXPR_CONCAT,            // expr expr
    AST_EXPR_LSHIFT,            // expr expr
    AST_EXPR_RSHIFT,            // expr expr
    AST_EXPR_ASHIFT,            // expr expr
    AST_EXPR_BITAND,            // expr expr
    AST_EXPR_BITXOR,            // expr expr
    AST_EXPR_BITOR,             // expr expr

    AST_OP_EQ,                  // -
    AST_OP_NE,                  // -
    AST_OP_LT,                  // -
    AST_OP_LE,                  // -
    AST_OP_GT,                  // -
    AST_OP_GE,                  // -
    AST_OP_IS,                  // -
    AST_OP_IS_NOT,              // -

    AST_EXPR_NULL,              // -
    AST_EXPR_FALSE,             // -
    AST_EXPR_TRUE,              // -
    AST_EXPR_NUMBER,            // leaf 0.0
    AST_EXPR_STRING,            // leaf "string"

    AST_EXPR_COMPARE,           // expr ( op expr )+
    AST_EXPR_NOT,               // expr
    AST_EXPR_AND,               // expr expr
    AST_EXPR_OR,                // expr expr
    AST_EXPR_IF,                // expr expr expr_elif* expr
    AST_EXPR_ELIF,              // expr expr

    AST_EXPR_KEY,               // expr leaf "key"
    AST_EXPR_INDEX,             // expr expr
    AST_EXPR_CALL,              // expr expr*
    AST_EXPR_UNPACK,            // last expression in list with ...
    AST_EXPR_ARRAY,             // expr*
    AST_EXPR_TABLE,             // keyval*
    AST_TABLE_KEY,              // expr expr
    AST_EXPR_YIELD,             // expr+
    AST_EXPR_YIELD_FOR,         // expr

    AST_FUNCTION,               // parameters block
    AST_PARAMETERS,             // name* vararg_param?
    AST_VARARG_PARAM,           // name

    AST_BLOCK,                  // stmt|call-expr|yield-expr*

    AST_STMT_VAR,               // name|name_list rval|rval_list?
    AST_DEFINITION,             // name|qual_name def
    AST_ASSIGN,                 // lval|lval_list rval|rval_list
    AST_OP_ASSIGN,              // lval [AST_OP_MUL] expr
    AST_NAME_LIST,              // name+
    AST_LVAL_LIST,              // expr+
    AST_RVAL_LIST,              // expr+

    AST_STMT_IF,                // expr block elif* block?
    AST_STMT_ELIF,              // expr block
    AST_STMT_FOR_STEP,          // name expr expr expr block
    AST_STMT_FOR_EACH,          // name|name_list expr block
    AST_STMT_WHILE,             // expr block
    AST_STMT_REPEAT,            // block expr
    AST_STMT_BREAK,             // -
    AST_STMT_CONTINUE,          // -
    AST_STMT_RETURN,            // expr*
    AST_STMT_THROW,             // expr

    AST_DEF_FUNCTION,           // leaf function
    AST_DEF_OBJECT,             // prototype? object_key|definition*
    AST_OBJECT_PROTOTYPE,       // expr
    AST_OBJECT_KEY,             // name expr

    AST_NAME,                   // leaf "name"
    AST_GLOBAL_NAME,            // Reference to global value.
    AST_UPVAL_NAME,             // Reference to upval.
    AST_LOCAL_DECL,             // Declaration of a local variable.
    AST_LOCAL_NAME,             // Reference to local variable.
    AST_UPVAL_NAME_SUPER,       // superof( upval ).
    AST_LOCAL_NAME_SUPER,       // superof( local variable ).
};

enum ast_node_leaf : uint8_t
{
    AST_NO_LEAF,                // No leaf data.
    AST_LEAF_STRING,            // String literal.
    AST_LEAF_NUMBER,            // Number literal.
    AST_LEAF_FUNCTION,          // Child function.
    AST_LEAF_INDEX,             // Index into function's upvals or locals, or block close index.
};

struct ast_node
{
    ast_node_kind kind;         // AST node kind.
    ast_node_leaf leaf;         // Is there associated leaf data?
    uint8_t prev_leaf;          // Does the previous node have leaf data?
    srcloc sloc;                // Source location.
    unsigned child_index;       // Index of first child, or invalid.
    unsigned next_index;        // Index of next sibling, fixed up afterwards.

    const ast_leaf_string& leaf_string() const;
    const ast_leaf_number& leaf_number() const;
    const ast_leaf_function& leaf_function() const;
    const ast_leaf_index& leaf_index() const;

    ast_leaf_string& leaf_string();
    ast_leaf_number& leaf_number();
    ast_leaf_function& leaf_function();
    ast_leaf_index& leaf_index();
};

struct ast_leaf_string
{
    const char* text;
    size_t size;
};

struct ast_leaf_number
{
    double n;
};

struct ast_leaf_function
{
    ast_function* function;
};

struct ast_leaf_index
{
    unsigned index;
};

inline const ast_leaf_string& ast_node::leaf_string() const
{
    static_assert( sizeof( ast_leaf_string ) <= sizeof( ast_node ) );
    assert( leaf == AST_LEAF_STRING );
    return *(const ast_leaf_string*)( this + 1 );
}

inline const ast_leaf_number& ast_node::leaf_number() const
{
    static_assert( sizeof( ast_leaf_number ) <= sizeof( ast_node ) );
    assert( leaf == AST_LEAF_NUMBER );
    return *(const ast_leaf_number*)( this + 1 );
}

inline const ast_leaf_function& ast_node::leaf_function() const
{
    static_assert( sizeof( ast_leaf_function ) <= sizeof( ast_node ) );
    assert( leaf == AST_LEAF_FUNCTION );
    return *(const ast_leaf_function*)( this + 1 );
}

inline const ast_leaf_index& ast_node::leaf_index() const
{
    static_assert( sizeof( ast_leaf_index ) <= sizeof( ast_node ) );
    assert( leaf == AST_LEAF_INDEX );
    return *(const ast_leaf_index*)( this + 1 );
}

inline ast_leaf_string& ast_node::leaf_string()
{
    static_assert( sizeof( ast_leaf_string ) <= sizeof( ast_node ) );
    assert( leaf == AST_LEAF_STRING );
    return *(ast_leaf_string*)( this + 1 );
}

inline ast_leaf_number& ast_node::leaf_number()
{
    static_assert( sizeof( ast_leaf_number ) <= sizeof( ast_node ) );
    assert( leaf == AST_LEAF_NUMBER );
    return *(ast_leaf_number*)( this + 1 );
}

inline ast_leaf_function& ast_node::leaf_function()
{
    static_assert( sizeof( ast_leaf_function ) <= sizeof( ast_node ) );
    assert( leaf == AST_LEAF_FUNCTION );
    return *(ast_leaf_function*)( this + 1 );
}

inline ast_leaf_index& ast_node::leaf_index()
{
    static_assert( sizeof( ast_leaf_index ) <= sizeof( ast_node ) );
    assert( leaf == AST_LEAF_INDEX );
    return *(ast_leaf_index*)( this + 1 );
}

}

#endif

