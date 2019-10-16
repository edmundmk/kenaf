//
//  code.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef CODE_H
#define CODE_H

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

    OP_NULL,            // r = null                 | K | r | - | - |
    OP_BOOL,            // r = c ? true : false     | K | r |   c   |
    OP_LDK,             // r = k[c]                 | K | r |   c   |

    OP_LEN,             // r = #a                   | A | r | a | - |
    OP_NEG,             // r = -a                   | A | r | a | - |
    OP_POS,             // r = +a                   | A | r | a | - |
    OP_BITNOT,          // r = ~a                   | A | r | a | - |
    OP_NOT,             // r = not a                | A | r | a | - |

    OP_ADD,             // r = a + b                | A | r | a | b |
    OP_ADDK,            // r = a + k[b]             | A | r | a | b |
    OP_ADDI,            // r = a + i                | A | r | a | i |
    OP_SUB,             // r = b - a                | A | r | a | b |
    OP_SUBK,            // r = k[b] - a             | A | r | a | b |
    OP_SUBI,            // r = i - a                | A | r | a | b |
    OP_MUL,             // r = a * b                | A | r | a | b |
    OP_MULK,            // r = a * k[b]             | A | r | a | b |
    OP_MULI,            // r = a * i                | A | r | a | i |
    OP_CONCAT,          // r = a ~ b                | A | r | a | b |
    OP_CONCATK,         // r = a ~ k[b]             | A | r | a | b |
    OP_RCONCATK,        // r = k[b] ~ a             | A | r | a | b |
    OP_DIV,             // r = a / b                | A | r | a | b |
    OP_INTDIV,          // r = a // b               | A | r | a | b |
    OP_MOD,             // r = a % b                | A | r | a | b |
    OP_LSHIFT,          // r = a << b               | A | r | a | b |
    OP_RSHIFT,          // r = a >> b               | A | r | a | b |
    OP_ASHIFT,          // r = a ~>> b              | A | r | a | b |
    OP_BITAND,          // r = a & b                | A | r | a | b |
    OP_BITXOR,          // r = a ^ b                | A | r | a | b |
    OP_BITOR,           // r = a | b                | A | r | a | b |

    OP_EQ,              // r = a == b               | A | r | a | b |
    OP_NE,              // r = a != b               | A | r | a | b |
    OP_LT,              // r = a < b                | A | r | a | b |
    OP_LE,              // r = a <= b               | A | r | a | b |
    OP_IS,              // r = a is b               | A | r | a | b |

    OP_JUMP,            // jump                     | J | - |   j   |
    OP_JT,              // if r then jump           | T | r |   j   |
    OP_JF,              // if not r then jump       | T | r |   j   |
    OP_JEQ,             // if a == b then jump      | T | - | a | b || J | - |   j   |
    OP_JEQK,            // if a == k[b] then jump   | T | - | a | b || J | - |   j   |
    OP_JNE,             // if a != b then jump      | T | - | a | b || J | - |   j   |
    OP_JNEK,            // if a != k[b] then jump   | T | - | a | b || J | - |   j   |
    OP_JLT,             // if a < b then jump       | T | - | a | b || J | - |   j   |
    OP_JLTK,            // if a < k[b] then jump    | T | - | a | b || J | - |   j   |
    OP_JGTK,            // if a > k[b] then jump    | T | - | a | b || J | - |   j   |
    OP_JLE,             // if a <= b then jump      | T | - | a | b || J | - |   j   |
    OP_JLEK,            // if a <= k[b] then jump   | T | - | a | b || J | - |   j   |
    OP_JGEK,            // if a >= k[b] then jump   | T | - | a | b || J | - |   j   |

    OP_GET_GLOBAL,      // r = s[c]                 | G | r |   c   |
    OP_GET_KEY,         // r = a[ s[b] ]            | G | r | a | b |
    OP_SET_KEY,         // a[ s[b] ] = r            | G | r | a | b |
    OP_GET_INDEX,       // r = a[ b ]               | G | r | a | b |
    OP_GET_INDEXK,      // r = a[ k[b] ]            | G | r | a | b |
    OP_GET_INDEXI,      // r = a[ %b ]              | G | r | a | b |
    OP_SET_INDEX,       // a[ b ] = r               | G | r | a | b |
    OP_SET_INDEXK,      // a[ k[b] ] = r            | G | r | a | b |
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
    OP_YCALLR,          // b = yield call( r:a )    | X | r | a | b |
    OP_YIELD,           // r:b = yield r:a          | X | r | a | b |
    OP_EXTEND,          // b.append( r:a )          | X | r | a | b |
    OP_RETURN,          // return r:a               | X | r | a | - |
    OP_VARARG,          // r:b = args ...           | X | r | - | b |
    OP_UNPACK,          // r:b = a ...              | X | r | a | b |

    OP_GENERATE,        // r,b = generate a         | F | r | a | b |
    OP_FOR_EACH,        // r:b = generate a,r'      | F | r | a | b || J | r'|   j   |
    OP_FOR_STEP,        // r = for step a,b,r'      | F | r | a | b || J | r'|   j   |

    OP_SUPER,           // r = super a              | G | r | a | - |
    OP_THROW,           // throw r                  | J | r | - | - |

    OP_FUNCTION,        // r = close function       | N | r |   c   |
    OP_F_VARENV,        // r.out[ %a ] = b          | G | r | a | b |
    OP_F_OUTENV,        // r.out[ %a ] = out[b]     | G | r | a | b |
};

struct op
{
    op();
    op( enum opcode opcode, uint8_t r, uint8_t a, uint8_t b );
    op( enum opcode opcode, uint8_t r, uint8_t a, int8_t i );
    op( enum opcode opcode, uint8_t r, uint16_t c );
    op( enum opcode opcode, uint8_t r, int16_t j );

    enum opcode opcode;
    uint8_t r;
    union
    {
        struct { uint8_t a; struct { uint8_t b; int8_t i; }; };
        uint16_t c;
        int16_t j;
    };
};

inline op::op() : opcode( OP_MOV ), r( 0 ), a( 0 ), b( 0 ) {}
inline op::op( enum opcode opcode, uint8_t r, uint8_t a, uint8_t b ) : opcode( opcode ), r( r ), a( a ), b( b ) {}
inline op::op( enum opcode opcode, uint8_t r, uint8_t a, int8_t i ) : opcode( opcode ), r( r ), a( a ), i( i ) {}
inline op::op( enum opcode opcode, uint8_t r, uint16_t c ) : opcode( opcode ), r( r ), c( c ) {}
inline op::op( enum opcode opcode, uint8_t r, int16_t j ) : opcode( opcode ), r( r ), j( j ) {}

const uint32_t CODE_MAGIC = 0x5B2A2A5D; // '[**]'

struct code_script
{
    uint32_t magic;
    uint32_t code_size;
    uint32_t script_name;
    uint32_t heap_size;
    uint32_t function_count;

    const char* heap() const;
    const code_function* functions() const;
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
    uint32_t op_count;
    uint16_t constant_count;
    uint16_t selector_count;
    uint8_t outenv_count;
    uint8_t param_count;
    uint8_t stack_size;
    uint8_t flags;

    const op* ops() const;
    const code_constant* constants() const;
    const code_selector* selectors() const;
    const code_debug_function* debug_function() const;
    const code_function* next() const;

    void debug_print( const code_script* script ) const;
};

struct code_constant
{
    uint64_t v;

    code_constant();
    explicit code_constant( double n );
    explicit code_constant( uint32_t s );

    bool is_number() const;
    double as_number() const;
    size_t as_offset() const;
};

inline code_constant::code_constant() {}
inline code_constant::code_constant( double n ) { uint64_t u; memcpy( &u, &n, sizeof( u ) ); v = ~u; }
inline code_constant::code_constant( uint32_t s ) { v = s; }
inline bool code_constant::is_number() const { return v > UINT_MAX; }
inline double code_constant::as_number() const { uint64_t u = ~v; double n; memcpy( &n, &u, sizeof( n ) ); return n; }
inline size_t code_constant::as_offset() const { return (uint32_t)v; }

struct code_selector
{
    uint32_t key;
};

struct code_debug_function
{
    uint32_t code_size;
    uint32_t function_name;
    uint32_t sloc_count;
    uint32_t newline_count;
    uint32_t variable_count;
    uint32_t var_span_count;
    uint32_t heap_size;

    const uint32_t* slocs() const;
    const uint32_t* newlines() const;
    const code_debug_variable* variables() const;
    const code_debug_var_span* var_spans() const;
    const char* heap() const;

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

