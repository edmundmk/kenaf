//
//  code.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_CODE_H
#define KF_CODE_H

/*
    This describes our bytecode, and a serialization of it.
*/

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

namespace kf
{

struct code_script;
struct code_function;
struct code_selector;
struct code_constant;
struct code_debug_function;
struct code_debug_variable;
struct code_debug_var_span;

enum opcode : uint8_t
{
    OP_MOV,             // r = a                    | M | r | a | - |
    OP_SWP,             // r <-> a                  | M | r | a | - |

    OP_LDV,             // r = null / true / false  | L | r |   c   |
    OP_LDK,             // r = k[c]                 | L | r |   c   |
    OP_LDJ,             // r = c                    | L | r |   c   |

    OP_LEN,             // r = #a                   | A | r | a | - |
    OP_NEG,             // r = -a                   | A | r | a | - |
    OP_POS,             // r = +a                   | A | r | a | - |
    OP_BITNOT,          // r = ~a                   | A | r | a | - |
    OP_NOT,             // r = not a                | A | r | a | - |

    OP_ADD,             // r = a + b                | A | r | a | b |
    OP_ADDN,            // r = a + k[b]             | A | r | a | b |
    OP_ADDI,            // r = a + i                | A | r | a | i |
    OP_SUB,             // r = b - a                | A | r | a | b |
    OP_SUBN,            // r = k[b] - a             | A | r | a | b |
    OP_SUBI,            // r = i - a                | A | r | a | b |
    OP_MUL,             // r = a * b                | A | r | a | b |
    OP_MULN,            // r = a * k[b]             | A | r | a | b |
    OP_MULI,            // r = a * i                | A | r | a | i |
    OP_CONCAT,          // r = a ~ b                | A | r | a | b |
    OP_CONCATS,         // r = a ~ k[b]             | A | r | a | b |
    OP_RCONCATS,        // r = k[b] ~ a             | A | r | a | b |
    OP_DIV,             // r = a / b                | A | r | a | b |
    OP_INTDIV,          // r = a // b               | A | r | a | b |
    OP_MOD,             // r = a % b                | A | r | a | b |
    OP_LSHIFT,          // r = a << b               | A | r | a | b |
    OP_RSHIFT,          // r = a >> b               | A | r | a | b |
    OP_ASHIFT,          // r = a ~>> b              | A | r | a | b |
    OP_BITAND,          // r = a & b                | A | r | a | b |
    OP_BITXOR,          // r = a ^ b                | A | r | a | b |
    OP_BITOR,           // r = a | b                | A | r | a | b |

    OP_IS,              // r = a is b               | A | r | a | b |

    OP_JMP,             // jump                     | J | - |   j   |
    OP_JT,              // if r then jump           | T | r |   j   |
    OP_JF,              // if not r then jump       | T | r |   j   |
    OP_JEQ,             // if a == b then jump      | T | ! | a | b || J | - |   j   |
    OP_JEQN,            // if a == k[b] then jump   | T | ! | a | b || J | - |   j   |
    OP_JEQS,            // if a == k[b] then jump   | T | ! | a | b || J | - |   j   |
    OP_JLT,             // if a < b then jump       | T | ! | a | b || J | - |   j   |
    OP_JLTN,            // if a < k[b] then jump    | T | ! | a | b || J | - |   j   |
    OP_JGTN,            // if a > k[b] then jump    | T | ! | a | b || J | - |   j   |
    OP_JLE,             // if a <= b then jump      | T | ! | a | b || J | - |   j   |
    OP_JLEN,            // if a <= k[b] then jump   | T | ! | a | b || J | - |   j   |
    OP_JGEN,            // if a >= k[b] then jump   | T | ! | a | b || J | - |   j   |

    OP_GET_GLOBAL,      // r = s[c]                 | G | r |   c   |
    OP_GET_KEY,         // r = a[ s[b] ]            | G | r | a | b |
    OP_SET_KEY,         // a[ s[b] ] = r            | G | r | a | b |
    OP_GET_INDEX,       // r = a[ b ]               | G | r | a | b |
    OP_GET_INDEXI,      // r = a[ %b ]              | G | r | a | b |
    OP_SET_INDEX,       // a[ b ] = r               | G | r | a | b |
    OP_SET_INDEXI,      // a[ %b ] = r              | G | r | a | b |

    OP_NEW_ENV,         // r = environment c        | N | r |   c   |
    OP_GET_VARENV,      // r = (env)a[ %b ]         | G | r | a | b |
    OP_SET_VARENV,      // (env)a[ %b ] = r         | G | r | a | b |
    OP_GET_OUTENV,      // r = out[a][ %b ]         | G | r | a | b |
    OP_SET_OUTENV,      // out[a][ %b ] = r         | G | r | a | b |

    OP_NEW_OBJECT,      // r = object proto         | N | r | a | - |
    OP_NEW_ARRAY,       // r = [], reserve c        | N | r |   c   |
    OP_NEW_TABLE,       // r = {}, reserve c        | N | r |   c   |
    OP_APPEND,          // r.append( a )            | G | r | a | - |

    OP_CALL,            // r:b = call( r:a )        | X | r | a | b |
    OP_CALLR,           // b = call( r:a )          | X | r | a | b |
    OP_YCALL,           // r:b = call( r:a )        | X | r | a | b |
    OP_YIELD,           // r:b = yield r:a          | X | r | a | b |
    OP_RETURN,          // return r:a               | X | r | a | - |
    OP_VARARG,          // r:b = args ...           | X | r | - | b |
    OP_UNPACK,          // r:b = a ...              | X | r | a | b |
    OP_EXTEND,          // b.append( r:a )          | X | r | a | b |

    OP_GENERATE,        // r:2 = generate a         | F | r | a | b |
    OP_FOR_EACH,        // r:b = generate a:2       | F | r | a | - || J | - |   j   |
    OP_FOR_STEP,        // r = for step a:3         | F | r | a | - || J | - |   j   |

    OP_SUPER,           // r = super a              | G | r | a | - |
    OP_THROW,           // throw r                  | J | r | - | - |

    OP_FUNCTION,        // r = close function       | N | r |   c   |
    OP_F_VARENV,        // r.out[ %a ] = b          | G | r | a | b |
    OP_F_OUTENV,        // r.out[ %a ] = out[b]     | G | r | a | b |
};

const uint8_t OP_STACK_MARK = 0xFF;

struct op
{
    static op op_ab( enum opcode opcode, uint8_t r, uint8_t a, uint8_t b );
    static op op_ai( enum opcode opcode, uint8_t r, uint8_t a, int8_t i );
    static op op_c( enum opcode opcode, uint8_t r, uint16_t c );
    static op op_j( enum opcode opcode, uint8_t r, int16_t j );

    enum opcode opcode;
    uint8_t r;
    union
    {
        struct { uint8_t a; union { uint8_t b; int8_t i; }; };
        uint16_t c;
        int16_t j;
    };
};

inline op op::op_ab( enum opcode opcode, uint8_t r, uint8_t a, uint8_t b ) { op o; o.opcode = opcode; o.r = r; o.a = a; o.b = b; return o; }
inline op op::op_ai( enum opcode opcode, uint8_t r, uint8_t a, int8_t i ) { op o; o.opcode = opcode; o.r = r; o.a = a; o.i = i; return o; }
inline op op::op_c( enum opcode opcode, uint8_t r, uint16_t c ) { op o; o.opcode = opcode; o.r = r; o.c = c; return o; }
inline op op::op_j( enum opcode opcode, uint8_t r, int16_t j ) { op o; o.opcode = opcode; o.r = r; o.j = j; return o; }

static_assert( sizeof( op ) == 4 );

const uint32_t CODE_MAGIC = 0x5D2A2A5B; // '[**]'

struct code_script
{
    uint32_t magic;
    uint32_t code_size;
    uint32_t function_size;
    uint32_t function_count;
    uint32_t heap_size;
    uint32_t debug_script_name;
    uint32_t debug_newline_count;
    uint32_t debug_heap_size;

    const code_function* functions() const;
    const char* heap() const;
    const uint32_t* debug_newlines() const;
    const char* debug_heap() const;

    void debug_print() const;
};

enum
{
    CODE_FLAGS_NONE         = 0,
    CODE_FLAGS_VARARGS      = 1 << 0,
    CODE_FLAGS_GENERATOR    = 1 << 1,
};

struct code_function
{
    uint32_t code_size;
    uint16_t op_count;
    uint16_t constant_count;
    uint16_t selector_count;
    uint16_t function_count;
    uint8_t outenv_count;
    uint8_t param_count;
    uint8_t stack_size;
    uint8_t code_flags;

    const op* ops() const;
    const code_constant* constants() const;
    const code_selector* selectors() const;
    const uint32_t* functions() const;
    const code_debug_function* debug_function() const;
    const code_function* next() const;

    void debug_print( const code_script* script ) const;
};

struct code_constant
{
    uint32_t text;
    uint32_t size;
    double n;
};

struct code_selector
{
    uint32_t text;
    uint32_t size;
};

struct code_debug_function
{
    uint32_t code_size;
    uint32_t function_name;
    uint32_t sloc_count;
    uint32_t variable_count;
    uint32_t var_span_count;

    const uint32_t* slocs() const;
    const code_debug_variable* variables() const;
    const code_debug_var_span* var_spans() const;

    void debug_print( const code_script* script ) const;
};

struct code_debug_variable
{
    uint32_t variable_name : 24;
    uint32_t r : 8;
};

struct code_debug_var_span
{
    uint32_t variable_index;
    uint32_t lower;
    uint32_t upper;
};

}

#endif

