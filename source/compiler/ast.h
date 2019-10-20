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
#include "index_vector.h"
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
struct ast_leaf_outenv;
struct ast_node_index;

const unsigned AST_INVALID_INDEX = ~(unsigned)0;

struct ast_script
{
    ast_script();
    ~ast_script();

    ast_function* new_function( srcloc sloc, ast_function* outer );
    void debug_print() const;

    index_vector< std::unique_ptr< ast_function >, 0xFFFF > functions;
};

struct ast_outenv
{
    unsigned outer_index;       // Index in outer function's outenvs or locals.
    bool outer_outenv;          // If true, outenv was an outenv for the outer function.
};

enum ast_local_kind : uint8_t
{
    LOCAL_VAR,
    LOCAL_PARAM,
    LOCAL_PARAM_SELF,
    LOCAL_PARAM_VARARG,
    LOCAL_FOR_EACH,
    LOCAL_FOR_STEP,
    LOCAL_TEMPORARY,
};

struct ast_local
{
    ast_local()
        :   varenv_index( AST_INVALID_INDEX )
        ,   varenv_slot( -1 )
        ,   kind( LOCAL_VAR )
    {
    }

    std::string_view name;      // Name of local or parameter.
    unsigned varenv_index;      // Index of local environment record.
    uint8_t varenv_slot;        // Slot in local environment record, or count of slots.
    ast_local_kind kind;        // Local kind.
};

struct ast_function
{
    ast_function( srcloc sloc, ast_function* outer, unsigned index );
    ~ast_function();

    void fixup_nodes();
    void debug_print() const;

    srcloc sloc;                // Source location of function.
    std::string name;           // Name of function.
    ast_function* outer;        // Lexically outer function.
    unsigned index;             // Index of function in the AST.
    unsigned parameter_count;   // First count locals are parameters.
    bool implicit_self;         // Does the function have implicit self?
    bool is_generator;          // Is it a generator?
    bool is_top_level;          // Is it the top-level function of a script?
    bool is_varargs;            // Does it have a varargs parameter?

    index_vector< ast_outenv, 0xFF > outenvs;
    index_vector< ast_local, 0xFFEF > locals;
    index_vector< ast_node, 0xFFFFFFFF > nodes;
};

enum ast_node_kind : uint16_t
{
    AST_NONE,

    // -- MUST MATCH IR OPS --
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
    // -- MUST MATCH IR OPS --

    AST_EXPR_NULL,              // -
    AST_EXPR_FALSE,             // -
    AST_EXPR_TRUE,              // -
    AST_EXPR_NUMBER,            // leaf 0.0
    AST_EXPR_STRING,            // leaf "string"

    AST_EXPR_COMPARE,           // expr ( op expr )+
    AST_OP_EQ,                  // -
    AST_OP_NE,                  // -
    AST_OP_LT,                  // -
    AST_OP_LE,                  // -
    AST_OP_GT,                  // -
    AST_OP_GE,                  // -
    AST_OP_IS,                  // -
    AST_OP_IS_NOT,              // -

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

    AST_DECL_VAR,               // name|name_list rval|rval_list?
    AST_DECL_DEF,               // name|qual_name def
    AST_RVAL_ASSIGN,            // lval|lval_list rval|rval_list
    AST_RVAL_OP_ASSIGN,         // lval [AST_OP_MUL] expr
    AST_NAME_LIST,              // name+
    AST_LVAL_LIST,              // expr+
    AST_RVAL_LIST,              // expr+

    AST_FUNCTION,               // parameters block
    AST_PARAMETERS,             // name* vararg_param?
    AST_VARARG_PARAM,           // name

    AST_BLOCK,                  // stmt|call-expr|yield-expr*

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
    AST_OBJKEY_DECL,            // Declares object key.
    AST_LOCAL_DECL,             // Declaration of a local variable (may be in varenv).
    AST_LOCAL_NAME,             // Reference to local (may be in varenv).
    AST_SUPER_NAME,             // superof( local ).
    AST_OUTENV_NAME,            // Name of value stored in outer environment.
    AST_GLOBAL_NAME,            // Name of global.
};

enum ast_node_leaf : uint8_t
{
    AST_NO_LEAF,                // No leaf data.
    AST_LEAF_STRING,            // String literal.
    AST_LEAF_NUMBER,            // Number literal.
    AST_LEAF_FUNCTION,          // Child function.
    AST_LEAF_INDEX,             // Index into function's locals.
    AST_LEAF_OUTENV,            // Index into function's outenvs, and outenv slot.
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
    const ast_leaf_outenv& leaf_outenv() const;

    ast_leaf_string& leaf_string();
    ast_leaf_number& leaf_number();
    ast_leaf_function& leaf_function();
    ast_leaf_index& leaf_index();
    ast_leaf_outenv& leaf_outenv();
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

struct ast_leaf_outenv
{
    unsigned outenv_index;
    unsigned outenv_slot;
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

inline const ast_leaf_outenv& ast_node::leaf_outenv() const
{
    static_assert( sizeof( ast_leaf_outenv ) <= sizeof( ast_node ) );
    assert( leaf == AST_LEAF_OUTENV );
    return *(const ast_leaf_outenv*)( this + 1 );
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

inline ast_leaf_outenv& ast_node::leaf_outenv()
{
    static_assert( sizeof( ast_leaf_outenv ) <= sizeof( ast_node ) );
    assert( leaf == AST_LEAF_OUTENV );
    return *(ast_leaf_outenv*)( this + 1 );
}

struct ast_node_index
{
    inline ast_node* operator -> () const { return node; }
    ast_node* node;
    unsigned index;
};

inline ast_node_index ast_child_node( ast_function* function, ast_node_index index )
{
    unsigned child_index = index.node->child_index;
    return { &function->nodes[ child_index ], child_index };
}

inline ast_node_index ast_next_node( ast_function* function, ast_node_index index )
{
    unsigned next_index = index.node->next_index;
    return { &function->nodes[ next_index ], next_index };
}

}

#endif

