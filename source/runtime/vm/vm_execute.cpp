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

static inline string_object* cast_string( value u )
{
    if ( is_object( u ) )
    {
        object* uo = as_object( u );
        if ( header( uo )->type == STRING_OBJECT )
        {
            return (string_object*)uo;
        }
    }
    return nullptr;
}

static lookup_object* keyer_of( vm_context* vm, value u )
{
    if ( is_number( u ) )
    {
        return vm->prototypes[ NUMBER_OBJECT ];
    }
    if ( u.v > 3 )
    {
        object* uo = as_object( u );
        type_code type = header( uo )->type;
        if ( type == LOOKUP_OBJECT )
        {
            return (lookup_object*)uo;
        }
        else
        {
            return vm->prototypes[ type ];
        }
    }
    if ( u.v > 0 )
    {
        return vm->prototypes[ BOOL_OBJECT ];
    }
    return nullptr;
}

static bool value_is( vm_context* vm, value u, value v )
{
    if ( is_number( v ) )
    {
        if ( is_number( u ) )
        {
            return as_number( u ) == as_number( v );
        }
        return false;
    }
    if ( u.v == v.v )
    {
        return true;
    }
    if ( v.v > 3 )
    {
        object* vo = as_object( v );
        type_code type = header( vo )->type;
        if ( type == LOOKUP_OBJECT )
        {
            lookup_object* prototype = (lookup_object*)vo;
            lookup_object* uo = keyer_of( vm, u );
            while ( uo )
            {
                if ( uo == prototype )
                {
                    return true;
                }
                uo = lookup_prototype( vm, uo );
            }
        }
        else if ( type == STRING_OBJECT )
        {
            if ( is_object( u ) )
            {
                object* uo = as_object( u );
                if ( header( uo )->type == STRING_OBJECT )
                {
                    string_object* us = (string_object*)uo;
                    string_object* vs = (string_object*)vo;
                    return us->size == vs->size && memcmp( us->text, vs->text, us->size ) == 0;
                }
            }
        }
    }
    return false;
}

void vm_execute( vm_context* vm )
{
    cothread_object* cothread = vm->cothreads.back();

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
    string_object* us;
    string_object* vs;

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

    case OP_LDV:
    {
        r[ op.r ] = { op.c };
        break;
    }

    case OP_LDK:
    {
        r[ op.r ] = read( program->constants[ op.c ] );
        break;
    }

    case OP_LDJ:
    {
        r[ op.r ] = number_value( op.j );
        break;
    }

    case OP_LEN:
    {
        value u = r[ op.a ];
        if ( is_object( u ) )
        {
            object* uo = as_object( u );
            type_code type = header( uo )->type;
            if ( type == ARRAY_OBJECT )
            {
                r[ op.r ] = number_value( ( (array_object*)uo )->length );
                break;
            }
            if ( type == TABLE_OBJECT )
            {
                r[ op.r ] = number_value( ( (table_object*)uo )->length );
                break;
            }
            if ( type == STRING_OBJECT )
            {
                r[ op.r ] = number_value( ( (string_object*)uo )->size );
                break;
            }
        }
        goto type_error;
    }

    case OP_NEG:
    {
        value u = r[ op.a ];
        if ( is_number( u ) )
        {
            r[ op.r ] = number_value( -as_number( u ) );
            break;
        }
        goto type_error;
    }

    case OP_POS:
    {
        value u = r[ op.a ];
        if ( is_number( u ) )
        {
            r[ op.r ] = number_value( +as_number( u ) );
            break;
        }
        goto type_error;
    }

    case OP_BITNOT:
    {
        value u = r[ op.a ];
        if ( is_number( u ) )
        {
            r[ op.r ] = number_value( ibitnot( as_number( u ) ) );
            break;
        }
        goto type_error;
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
        goto type_error;
    }

    case OP_ADD:
    {
        value v = r[ op.b ];
        if ( is_number( v ) )
        {
            n = as_number( v );
            goto op_add;
        }
        goto type_error;
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
        goto type_error;
    }

    case OP_SUB:
    {
        value v = r[ op.b ];
        if ( is_number( v ) )
        {
            n = as_number( v );
            goto op_sub;
        }
        goto type_error;
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
        goto type_error;
    }

    case OP_MUL:
    {
        value v = r[ op.b ];
        if ( is_number( v ) )
        {
            n = as_number( v );
            goto op_mul;
        }
        goto type_error;
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

    op_concat:
    {
        string_object* s = string_new( vm, nullptr, us->size + vs->size );
        memcpy( s->text, us->text, us->size );
        memcpy( s->text + us->size, vs->text, vs->size );
        r[ op.r ] = object_value( s );
        break;
    }

    op_concats:
    {
        value u = r[ op.a ];
        if ( ( us = cast_string( u ) ) )
        {
            goto op_concat;
        }
        goto type_error;
    }

    case OP_CONCAT:
    {
        value v = r[ op.b ];
        if ( ( vs = cast_string( v ) ) )
        {
            goto op_concats;
        }
        goto type_error;
    }

    case OP_CONCATS:
    {
        vs = (string_object*)as_object( read( k[ op.b ] ) );
        goto op_concats;
    }

    case OP_RCONCATS:
    {
        value v = r[ op.a ];
        us = (string_object*)as_object( read( k[ op.b ] ) );
        if ( ( vs = cast_string( v ) ) )
        {
            goto op_concat;
        }
        goto type_error;
    }

    case OP_DIV:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_number( u ) && is_number( v ) )
        {
            r[ op.r ] = number_value( as_number( u ) / as_number( v ) );
            break;
        }
        goto type_error;
    }

    case OP_INTDIV:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_number( u ) && is_number( v ) )
        {
            r[ op.r ] = number_value( ifloordiv( as_number( u ), as_number( v ) ) );
            break;
        }
        goto type_error;
    }

    case OP_MOD:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_number( u ) && is_number( v ) )
        {
            r[ op.r ] = number_value( ifloormod( as_number( u ), as_number( v ) ) );
            break;
        }
        goto type_error;
    }

    case OP_LSHIFT:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_number( u ) && is_number( v ) )
        {
            r[ op.r ] = number_value( ilshift( as_number( u ), as_number( v ) ) );
            break;
        }
        goto type_error;
    }

    case OP_RSHIFT:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_number( u ) && is_number( v ) )
        {
            r[ op.r ] = number_value( irshift( as_number( u ), as_number( v ) ) );
            break;
        }
        goto type_error;
    }

    case OP_ASHIFT:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_number( u ) && is_number( v ) )
        {
            r[ op.r ] = number_value( iashift( as_number( u ), as_number( v ) ) );
            break;
        }
        goto type_error;
    }

    case OP_BITAND:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_number( u ) && is_number( v ) )
        {
            r[ op.r ] = number_value( ibitand( as_number( u ), as_number( v ) ) );
            break;
        }
        goto type_error;
    }

    case OP_BITXOR:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_number( u ) && is_number( v ) )
        {
            r[ op.r ] = number_value( ibitxor( as_number( u ), as_number( v ) ) );
            break;
        }
        goto type_error;
    }

    case OP_BITOR:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_number( u ) && is_number( v ) )
        {
            r[ op.r ] = number_value( ibitor( as_number( u ), as_number( v ) ) );
            break;
        }
        goto type_error;
    }

    case OP_IS:
    {
        r[ op.r ] = value_is( vm, r[ op.a ], r[ op.b ] ) ? true_value : false_value;
        break;
    }

    case OP_JMP:
    {
        ip += op.j;
        break;
    }

    case OP_JT:
    {
        if ( test( r[ op.r ] ) )
        {
            ip += op.j;
        }
        break;
    }

    case OP_JF:
    {
        if ( ! test( r[ op.r ] ) )
        {
            ip += op.j;
        }
        break;
    }

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
    case OP_GET_INDEXI:
    case OP_SET_INDEX:
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

    return;

type_error:
    throw std::exception();
}

}

