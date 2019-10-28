//
//  vm_execute.cpp
//
//  Created by Edmund Kapusniak on 24/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "vm_execute.h"
#include "vm_context.h"
#include "../../common/code.h"
#include "../../common/imath.h"
#include "../objects/lookup_object.h"
#include "../objects/string_object.h"
#include "../objects/array_object.h"
#include "../objects/table_object.h"
#include "../objects/cothread_object.h"
#include "../objects/function_object.h"

namespace kf
{

static inline bool test( value u )
{
    // All values test true except null, false, -0.0, and +0.0.
    return u.v > 1
        && u.v != UINT64_C( 0x7FFF'FFFF'FFFF'FFFF )
        && u.v != UINT64_C( 0xFFFF'FFFF'FFFF'FFFF );
}

void vm_execute( vm_context* context )
{
    cothread_object* cothread = context->cothreads.back();

    vm_stack_frame stack_frame = cothread->stack_frames.back();
    function_object* function = stack_frame.function;
    unsigned ip = stack_frame.ip;

    program_object* program = read( function->program );
    const op* ops = program->ops;

    value* r = cothread->stack.data() + stack_frame.fp;
    ref_value* k = program->constants;

    while ( true )
    {
    double n;

    op op = ops[ ip++ ];
    switch ( op.opcode )
    {
    case OP_MOV:
    {
        r[ op.r ] = r[ op.a ];
        break;
    }

    case OP_SWP:
    {
        value w = r[ op.r ];
        r[ op.r ] = r[ op.a ];
        r[ op.a ] = w;
        break;
    }

    case OP_NULL:
    {
        r[ op.r ] = null_value;
        break;
    }

    case OP_BOOL:
    {
        r[ op.r ] = op.c ? true_value : false_value;
        break;
    }

    case OP_LDK:
    {
        r[ op.r ] = read( program->constants[ op.c ] );
        break;
    }

    case OP_LEN:
    {
        value u = r[ op.a ];
        if ( is_object( u ) )
        {
            object* object = as_object( u );
            type_code type = header( object )->type;
            if ( type == ARRAY_OBJECT )
            {
                r[ op.r ] = number_value( ( (array_object*)object )->length );
                break;
            }
            if ( type == TABLE_OBJECT )
            {
                r[ op.r ] = number_value( ( (table_object*)object )->length );
                break;
            }
        }
        throw std::exception();
    }

    case OP_NEG:
    {
        value u = r[ op.a ];
        if ( is_number( u ) )
        {
            r[ op.r ] = number_value( -as_number( u ) );
            break;
        }
        throw std::exception();
    }

    case OP_POS:
    {
        value u = r[ op.a ];
        if ( is_number( u ) )
        {
            r[ op.r ] = number_value( +as_number( u ) );
            break;
        }
        throw std::exception();
    }

    case OP_BITNOT:
    {
        value u = r[ op.a ];
        if ( is_number( u ) )
        {
            r[ op.r ] = number_value( ibitnot( as_number( u ) ) );
            break;
        }
        throw std::exception();
    }

    case OP_NOT:
    {
        value u = r[ op.a ];
        r[ op.r ] = test( u ) ? false_value : true_value;
        break;
    }

    op_add:
    {
        value u = r[ op.a ];
        if ( is_number( u ) )
        {
            r[ op.r ] = number_value( as_number( u ) + n );
            break;
        }
        throw std::exception();
    }

    case OP_ADD:
    {
        value v = r[ op.b ];
        if ( is_number( v ) )
        {
            n = as_number( v );
            goto op_add;
        }
        throw std::exception();
    }

    case OP_ADDN:
    {
        n = as_number( read( k[ op.b ] ) );
        goto op_add;
    }

    case OP_ADDI:
    {
        n = op.i;
        goto op_add;
    }

    op_sub:
    {
        value u = r[ op.a ];
        if ( is_number( u ) )
        {
            r[ op.r ] = number_value( n - as_number( u ) );
            break;
        }
        throw std::exception();
    }

    case OP_SUB:
    {
        value v = r[ op.b ];
        if ( is_number( v ) )
        {
            n = as_number( v );
            goto op_sub;
        }
        throw std::exception();
    }

    case OP_SUBN:
    {
        n = as_number( read( k[ op.b ] ) );
        goto op_sub;
    }

    case OP_SUBI:
    {
        n = op.i;
        goto op_sub;
    }

    op_mul:
    {
        value u = r[ op.a ];
        if ( is_number( u ) )
        {
            r[ op.r ] = number_value( as_number( u ) * n );
            break;
        }
        throw std::exception();
    }

    case OP_MUL:
    {
        value v = r[ op.b ];
        if ( is_number( v ) )
        {
            n = as_number( v );
            goto op_mul;
        }
        throw std::exception();
    }

    case OP_MULN:
    {
        n = as_number( read( k[ op.b ] ) );
        goto op_mul;
    }

    case OP_MULI:
    {
        n = op.i;
        goto op_mul;
    }

    case OP_CONCAT:
    case OP_CONCATS:
    case OP_RCONCATS:
    case OP_DIV:
    case OP_INTDIV:
    case OP_MOD:
    case OP_LSHIFT:
    case OP_RSHIFT:
    case OP_ASHIFT:
    case OP_BITAND:
    case OP_BITXOR:
    case OP_BITOR:


    case OP_IS:
    case OP_JMP:
    case OP_JT:
    case OP_JF:
    case OP_JEQ:
    case OP_JEQN:
    case OP_JEQS:
    case OP_JLT:
    case OP_JLTN:
    case OP_JGTN:
    case OP_JLE:
    case OP_JLEN:
    case OP_JGEN:
    case OP_GET_GLOBAL:
    case OP_GET_KEY:
    case OP_SET_KEY:
    case OP_GET_INDEX:
    case OP_GET_INDEXK:
    case OP_GET_INDEXI:
    case OP_SET_INDEX:
    case OP_SET_INDEXK:
    case OP_SET_INDEXI:
    case OP_NEW_ENV:
    case OP_GET_VARENV:
    case OP_SET_VARENV:
    case OP_GET_OUTENV:
    case OP_SET_OUTENV:
    case OP_NEW_OBJECT:
    case OP_NEW_ARRAY:
    case OP_NEW_TABLE:
    case OP_APPEND:
    case OP_CALL:
    case OP_CALLR:
    case OP_YCALL:
    case OP_YIELD:
    case OP_RETURN:
    case OP_VARARG:
    case OP_UNPACK:
    case OP_EXTEND:
    case OP_GENERATE:
    case OP_FOR_EACH:
    case OP_FOR_STEP:
    case OP_SUPER:
    case OP_THROW:

    case OP_FUNCTION:
    case OP_F_VARENV:
    case OP_F_OUTENV:
        break;
    }

    }

}

}

