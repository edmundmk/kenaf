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
    OP_BOOL,            // r = true/false           | K | r |   k   |
    OP_K,               // r = k                    | K | r |   k   |
    OP_I,               // r = i                    | K | r |   i   |

    OP_LENGTH,          // r = #a                   | A | r | a | - |
    OP_NEG,             // r = -a                   | A | r | a | - |
    OP_POS,             // r = +a                   | A | r | a | - |
    OP_BITNOT,          // r = ~a                   | A | r | a | - |
    OP_NOT,             // r = not a                | A | r | a | - |

    OP_ADD,             // r = a + b                | A | r | a | b |
    OP_ADD_K,           // r = a + k                | A | r | a | k |
    OP_ADD_I,           // r = a + i                | A | r | a | i |
    OP_MUL,             // r = a * b                | A | r | a | b |
    OP_MUL_K,           // r = a * k                | A | r | a | k |
    OP_MUL_I,           // r = a * i                | A | r | a | i |
    OP_SUB,             // r = a - b                | A | r | a | b |
    OP_SUB_K,           // r = a - k                | A | r | a | k |
    OP_RSUB_K,          // r = k - a                | A | r | a | k |
    OP_CONCAT,          // r = a ~ b                | A | r | a | b |
    OP_CONCAT_K,        // r = a ~ k                | A | r | a | k |
    OP_RCONCAT_K,       // r = k ~ a                | A | r | a | k |
    OP_DIV,             // r = a / b                | A | r | a | b |
    OP_INTDIV,          // r = a // b               | A | r | a | b |
    OP_MOD,             // r = a % b                | A | r | a | b |
    OP_LSHIFT,          // r = a << b               | A | r | a | b |
    OP_RSHIFT,          // r = a >> b               | A | r | a | b |
    OP_ASHIFT,          // r = a ~>> b              | A | r | a | b |
    OP_BITAND,          // r = a & b                | A | r | a | b |
    OP_BITXOR,          // r = a ^ b                | A | r | a | b |
    OP_BITOR,           // r = a | b                | A | r | a | b |

    OP_JUMP,            // jump                     | J | - |   j   |
    OP_CLOSE,           // close upstack, jump      | J | u |   j   |
    OP_TEST,            // if r then jump           | T | r |   j   |
    OP_EQ,              // if a == b then jump      | T | - | a | b || J | - |   j   |
    OP_EQ_K,            // if a == k then jump      | T | - | a | k || J | - |   j   |
    OP_EQ_V,            // r = a == b               | A | r | a | b |
    OP_NE,              // if a != b then jump      | T | - | a | b || J | - |   j   |
    OP_NE_K,            // if a != k then jump      | T | - | a | k || J | - |   j   |
    OP_NE_V,            // r = a != b               | A | r | a | b |
    OP_LT,              // if a < b then jump       | T | - | a | b || J | - |   j   |
    OP_LT_K,            // if a < k then jump       | T | - | a | k || J | - |   j   |
    OP_GT_K,            // if a > k then jump       | T | - | a | k || J | - |   j   |
    OP_LT_V,            // r = a < b                | A | r | a | b |
    OP_LE,              // if a <= b then jump      | T | - | a | b || J | - |   j   |
    OP_LE_K,            // if a <= k then jump      | T | - | a | k || J | - |   j   |
    OP_GE_K,            // if a >= k then jump      | T | - | a | k || J | - |   j   |
    OP_LE_V,            // r = a <= b               | A | r | a | b |
    OP_IS,              // r = a is b               | A | r | a | b |

    OP_GET_GLOBAL,      // r = globals[ name ]      | G | r |   k   |
    OP_GET_UPVAL,       // r = upval[ u ]           | G | r | u | - |
    OP_SET_UPVAL,       // upval[ u ] = r           | G | r | u | - |
    OP_GET_KEY,         // r = a[ selector ]        | G | r | a | s |
    OP_SET_KEY,         // a[ selector ] = r        | G | r | a | s |
    OP_GET_INDEX,       // r = a[ b ]               | G | r | a | b |
    OP_GET_INDEX_K,     // r = a[ k ]               | G | r | a | k |
    OP_GET_INDEX_I,     // r = a[ i ]               | G | r | a | u |
    OP_SET_INDEX,       // a[ b ] = r               | G | r | a | b |
    OP_SET_INDEX_K,     // a[ k ] = r               | G | r | a | k |
    OP_SET_INDEX_I,     // a[ i ] = r               | G | r | a | u |

    OP_NEW_OBJECT,      // r = object proto         | N | r | a | - |
    OP_NEW_ARRAY,       // r = []                   | N | r |   c   |
    OP_NEW_TABLE,       // r = {}                   | N | r |   c   |
    OP_APPEND,          // r.append( a )            | G | r | a | - |

    OP_FUNCTION,        // r = close function       | N | r |   k   |
    OP_UPVAL,           // r->[ u ] = new upval     | F | r | u | u |
    OP_UCOPY,           // r->[ u ] = upval[ u ]    | F | r | u | u |

    OP_CALL_V,          // b = call( r:a )          | X | r | a | b |
    OP_CALL_X,          // r:b = call( r:a )        | X | r | a | b |
    OP_YCALL_V,         // b = yield call( r:a )    | X | r | a | b |
    OP_YCALL_X,         // r:b = call( r:a )        | X | r | a | b |
    OP_YIELD,           // r:b = yield r:a          | X | r | a | b |
    OP_EXTEND,          // b.append( r:a )          | X | r | a | b |
    OP_RETURN,          // return r:a               | X | r | a | - |
    OP_VARARG,          // r:b = args ...           | X | r | - | b |
    OP_UNPACK,          // r:b = a ...              | X | r | a | b |

    OP_GENERATE,        // r,b = generate a         | F | r | a | b |
    OP_FOR_EACH,        // r:b = generate q,a       | F | r | a | b || J | q |   j   |
    OP_FOR_STEP,        // r = for step a,b,q       | F | r | a | b || J | q |   j   |

    OP_SUPER,           // r = super a              | G | r | a | - |
    OP_THROW,           // throw r                  | J | r | - | - |
};


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

