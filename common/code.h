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

#include <stdint.h>

namespace kf
{

enum
{
    OP_MOV,             // r = a                    | M | r | a | - |
    OP_SWP,             // r <-> a                  | M | r | a | - |

    OP_NULL,            // r = null                 | K | r | - | - |
    OP_BOOL,            // r = c ? true : false     | K | r |   c   |
    OP_K,               // r = k[c]                 | K | r |   c   |
    OP_I,               // r = j                    | K | r |   j   |

    OP_LENGTH,          // r = #a                   | A | r | a | - |
    OP_NEG,             // r = -a                   | A | r | a | - |
    OP_POS,             // r = +a                   | A | r | a | - |
    OP_BITNOT,          // r = ~a                   | A | r | a | - |
    OP_NOT,             // r = not a                | A | r | a | - |

    OP_ADD,             // r = a + b                | A | r | a | b |
    OP_ADDK,            // r = a + k[b]             | A | r | a | b |
    OP_ADDI,            // r = a + i                | A | r | a | i |
    OP_MUL,             // r = a * b                | A | r | a | b |
    OP_MULK,            // r = a * k[b]             | A | r | a | b |
    OP_MULI,            // r = a * i                | A | r | a | i |
    OP_SUB,             // r = a - b                | A | r | a | b |
    OP_SUBK,            // r = a - k[b]             | A | r | a | b |
    OP_RSUBK,           // r = k[b] - a             | A | r | a | b |
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
    OP_JCLOSE,          // close upstack, jump      | J | r |   j   |
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

    OP_GET_GLOBAL,      // r = g[ c ]               | G | r |   c   |
    OP_GET_UPVAL,       // r = u[ a ]               | G | r | a | - |
    OP_SET_UPVAL,       // u[ a ] = r               | G | r | a | - |
    OP_GET_KEY,         // r = s[ b ]               | G | r | a | b |
    OP_SET_KEY,         // s[ b ] = r               | G | r | a | b |
    OP_GET_INDEX,       // r = a[ b ]               | G | r | a | b |
    OP_GET_INDEXK,      // r = a[ k[b] ]            | G | r | a | b |
    OP_GET_INDEXI,      // r = a[ %b ]              | G | r | a | b |
    OP_SET_INDEX,       // a[ b ] = r               | G | r | a | b |
    OP_SET_INDEXK,      // a[ k[b] ] = r            | G | r | a | b |
    OP_SET_INDEXI,      // a[ %b ] = r              | G | r | a | b |

    OP_NEW_OBJECT,      // r = object proto         | N | r | a | - |
    OP_NEW_ARRAY,       // r = [], reserve c        | N | r |   c   |
    OP_NEW_TABLE,       // r = {}, reserve c        | N | r |   c   |
    OP_APPEND,          // r.append( a )            | G | r | a | - |

    OP_FUNCTION,        // r = close function       | N | r |   c   |
    OP_UPVAL,           // r->[ a ] = new upval b   | F | r | a | b |
    OP_UCOPY,           // r->[ a ] = u[ b ]        | F | r | a | b |

    OP_CALL,            // b = call( r:a )          | X | r | a | b |
    OP_CALLX,           // r:b = call( r:a )        | X | r | a | b |
    OP_YCALL,           // b = yield call( r:a )    | X | r | a | b |
    OP_YCALLX,          // r:b = call( r:a )        | X | r | a | b |
    OP_YIELD,           // r:b = yield r:a          | X | r | a | b |
    OP_EXTEND,          // b.append( r:a )          | X | r | a | b |
    OP_RETURN,          // return r:a               | X | r | a | - |
    OP_VARARG,          // r:b = args ...           | X | r | - | b |
    OP_UNPACK,          // r:b = a ...              | X | r | a | b |

    OP_GENERATE,        // r,b = generate a         | F | r | a | b |
    OP_FOR_EACH,        // r:b = generate r',a      | F | r | a | b || J | r'|   j   |
    OP_FOR_STEP,        // r = for step a,b,r'      | F | r | a | b || J | r'|   j   |

    OP_SUPER,           // r = super a              | G | r | a | - |
    OP_THROW,           // throw r                  | J | r | - | - |
};

struct op
{
    op();
    op( opcode opcode, uint8_t r, uint8_t a, uint8_t b );
    op( opcode opcode, uint8_t r, uint8_t a, int8_t i );
    op( opcode opcode, uint8_t r, uint16_t c );
    op( opcode opcode, uint8_t r, int16_t j );

    uint8_t opcode;
    uint8_t r;
    union
    {
        struct { uint8_t a; struct { uint8_t b; int8_t i; }; }
        uint16_t c;
        int16_t j;
    };
};

inline op::op() : opcode( OP_MOV ), r( 0 ), a( 0 ), b( 0 ) {}
inline op::op( opcode opcode, uint8_t r, uint8_t a, uint8_t b ) : opcode( opcode ), r( r ), a( a ), b( b ) {}
inline op::op( opcode opcode, uint8_t r, uint8_t a, int8_t i ) : opcode( opcode ), r( r ), a( a ), i( i ) {}
inline op::op( opcode opcode, uint8_t r, uint16_t c ) : opcode( opcode ), r( r ), c( c ) {}
inline op::op( opcode opcode, uint8_t r, int16_t j ) : opcode( opcode ), r( r ), j( j ) {}

const uint32_t CODE_MAGIC = 0x5B2A2A5D; // '[**]'

struct code_script
{
};

struct code_function
{
};

struct code_debug_function
{
};

struct code_debug_variable
{
};

struct code_debug_var_span
{
};

}

#endif

