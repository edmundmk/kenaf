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
    return u.v > 1 && u.v != number_value( +0.0 ).v && u.v != number_value( -0.0 ).v;
}

static inline bool string_equal( string_object* us, string_object* vs )
{
    return us == vs || ( us->size == vs->size && memcmp( us->text, vs->text, us->size ) == 0 );
}

static inline int string_compare( string_object* us, string_object* vs )
{
    if ( us == vs ) return 0;
    size_t size = std::min( us->size, vs->size );
    int order = memcmp( us->text, vs->text, size );
    if ( order != 0 ) return order;
    if ( us->size < vs->size ) return -1;
    if ( us->size > vs->size ) return +1;
    return 0;
}

static lookup_object* keyer_of( vm_context* vm, value u )
{
    if ( is_number( u ) )
    {
        return vm->prototypes[ NUMBER_OBJECT ];
    }
    else if ( is_string( u ) )
    {
        return vm->prototypes[ STRING_OBJECT ];
    }
    else if ( is_object( u ) )
    {
        type_code type = header( as_object( u ) )->type;
        if ( type == LOOKUP_OBJECT )
        {
            return (lookup_object*)as_object( u );
        }
        else
        {
            return vm->prototypes[ type ];
        }
    }
    else if ( is_bool( u ) )
    {
        return vm->prototypes[ BOOL_OBJECT ];
    }
    return nullptr;
}

static bool value_is( vm_context* vm, value u, value v )
{
    if ( is_number( v ) )
    {
        return is_number( u ) && as_number( u ) == as_number( v );
    }
    else if ( u.v == v.v )
    {
        return true;
    }
    else if ( is_object( v ) )
    {
        type_code type = header( as_object( v ) )->type;
        if ( type == LOOKUP_OBJECT )
        {
            lookup_object* vo = (lookup_object*)as_object( v );
            lookup_object* uo = keyer_of( vm, u );
            while ( uo )
            {
                if ( uo == vo ) return true;
                uo = lookup_prototype( vm, uo );
            }
        }
    }
    else if ( is_string( v ) )
    {
        return is_string( u ) && string_equal( as_string( u ), as_string( v ) );
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
    ref_value* k = program->constants;
    key_selector* s = program->selectors;

    value* r = cothread->stack.data() + stack_frame.fp;

    while ( true )
    {

    double n;
    string_object* us;
    string_object* vs;

    struct op op = ops[ ip++ ];
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
        r[ op.r ] = read( k[ op.c ] );
        break;
    }

    case OP_LEN:
    {
        value u = r[ op.a ];
        if ( is_object( u ) )
        {
            type_code type = header( as_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                r[ op.r ] = number_value( ( (array_object*)as_object( u ) )->length );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                r[ op.r ] = number_value( ( (table_object*)as_object( u ) )->length );
                break;
            }
        }
        else if ( is_string( u ) )
        {
            r[ op.r ] = number_value( as_string( u )->size );
            break;
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

    op_concat:
    {
        string_object* s = string_new( vm, nullptr, us->size + vs->size );
        memcpy( s->text, us->text, us->size );
        memcpy( s->text + us->size, vs->text, vs->size );
        r[ op.r ] = string_value( s );
        break;
    }

    op_concatu:
    {
        value u = r[ op.a ];
        if ( is_string( u ) )
        {
            us = as_string( u );
            goto op_concat;
        }
        goto type_error;
    }

    case OP_CONCAT:
    {
        value v = r[ op.b ];
        if ( is_string( v ) )
        {
            vs = as_string( v );
            goto op_concatu;
        }
        goto type_error;
    }

    case OP_CONCATS:
    {
        vs = as_string( read( k[ op.b ] ) );
        goto op_concatu;
    }

    case OP_RCONCATS:
    {
        value v = r[ op.a ];
        us = as_string( read( k[ op.b ] ) );
        if ( is_string( v ) )
        {
            vs = as_string( v );
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
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        bool test = false;
        if ( is_number( u ) )
        {
            test = is_number( v ) && as_number( u ) == as_number( v );
        }
        else if ( u.v == v.v )
        {
            test = true;
        }
        else if ( is_string( u ) )
        {
            test = is_string( v ) && string_equal( as_string( u ), as_string( v ) );
        }
        struct op jop = ops[ ip++ ];
        if ( test == op.r )
        {
            ip += jop.j;
        }
        break;
    }

    case OP_JEQN:
    {
        value u = r[ op.a ];
        bool test = is_number( u ) && as_number( u ) == as_number( read( k[ op.b ] ) );
        struct op jop = ops[ ip++ ];
        if ( test == op.r )
        {
            ip += jop.j;
        }
        break;
    }

    case OP_JEQS:
    {
        value u = r[ op.a ];
        bool test = is_string( u ) && string_equal( as_string( u ), as_string( read( k[ op.b ] ) ) );
        struct op jop = ops[ ip++ ];
        if ( test == op.r )
        {
            ip += jop.j;
        }
        break;
    }

    case OP_JLT:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        bool test = false;
        if ( is_number( u ) )
        {
            if ( ! is_number( v ) ) goto type_error;
            test = as_number( u ) < as_number( v );
        }
        else if ( is_string( u ) )
        {
            if ( ! is_string( v ) ) goto type_error;
            test = string_compare( as_string( u ), as_string( v ) ) < 0;
        }
        else
        {
            goto type_error;
        }
        struct op jop = ops[ ip++ ];
        if ( test == op.r )
        {
            ip += jop.j;
        }
        break;
    }

    case OP_JLTN:
    {
        value u = r[ op.a ];
        if ( ! is_number( u ) ) goto type_error;
        bool test = as_number( u ) < as_number( read( k[ op.b ] ) );
        struct op jop = ops[ ip++ ];
        if ( test == op.r )
        {
            ip += jop.j;
        }
        break;
    }

    case OP_JGTN:
    {
        value u = r[ op.a ];
        if ( ! is_number( u ) ) goto type_error;
        bool test = as_number( u ) > as_number( read( k[ op.b ] ) );
        struct op jop = ops[ ip++ ];
        if ( test == op.r )
        {
            ip += jop.j;
        }
        break;
    }

    case OP_JLE:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        bool test = false;
        if ( is_number( u ) )
        {
            if ( ! is_number( v ) ) goto type_error;
            test = as_number( u ) <= as_number( v );
        }
        else if ( is_string( u ) )
        {
            if ( ! is_string( v ) ) goto type_error;
            test = string_compare( as_string( u ), as_string( v ) ) <= 0;
        }
        else
        {
            goto type_error;
        }
        struct op jop = ops[ ip++ ];
        if ( test == op.r )
        {
            ip += jop.j;
        }
        break;
    }

    case OP_JLEN:
    {
        value u = r[ op.a ];
        if ( ! is_number( u ) ) goto type_error;
        bool test = as_number( u ) <= as_number( read( k[ op.b ] ) );
        struct op jop = ops[ ip++ ];
        if ( test == op.r )
        {
            ip += jop.j;
        }
        break;
    }

    case OP_JGEN:
    {
        value u = r[ op.a ];
        if ( ! is_number( u ) ) goto type_error;
        bool test = as_number( u ) >= as_number( read( k[ op.b ] ) );
        struct op jop = ops[ ip++ ];
        if ( test == op.r )
        {
            ip += jop.j;
        }
        break;
    }

    case OP_GET_GLOBAL:
    {
        key_selector* ks = s + op.c;
        r[ op.r ] = lookup_getkey( vm, vm->global_object, read( ks->key ), &ks->sel );
        break;
    }

    case OP_GET_KEY:
    {
        value u = r[ op.a ];
        key_selector* ks = s + op.b;
        r[ op.r ] = lookup_getkey( vm, keyer_of( vm, u ), read( ks->key ), &ks->sel );
        break;
    }

    case OP_SET_KEY:
    {
        value u = r[ op.a ];
        key_selector* ks = s + op.b;
        if ( ! is_object( u ) || header( as_object( u ) )->type != LOOKUP_OBJECT ) goto type_error;
        lookup_setkey( vm, (lookup_object*)as_object( u ), read( ks->key ), &ks->sel, r[ op.r ] );
        break;
    }

    case OP_GET_INDEX:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_object( u ) )
        {
            type_code type = header( as_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                array_object* array = (array_object*)as_object( u );
                if ( ! is_number( v ) ) goto type_error;
                r[ op.r ] = array_getindex( vm, array, (size_t)(int64_t)as_number( v ) );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                table_object* table = (table_object*)as_object( u );
                r[ op.r ] = table_getindex( vm, table, v );
                break;
            }
        }
        else if ( is_string( u ) )
        {
            // TODO.
        }
        goto type_error;
    }

    case OP_GET_INDEXI:
    {
        value u = r[ op.a ];
        if ( is_object( u ) )
        {
            type_code type = header( as_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                array_object* array = (array_object*)as_object( u );
                r[ op.r ] = array_getindex( vm, array, op.b );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                table_object* table = (table_object*)as_object( u );
                r[ op.r ] = table_getindex( vm, table, number_value( op.b ) );
                break;
            }
        }
        else if ( is_string( u ) )
        {
            // TODO.
        }
        goto type_error;
    }

    case OP_SET_INDEX:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( is_object( u ) )
        {
            type_code type = header( as_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                array_object* array = (array_object*)as_object( u );
                if ( ! is_number( v ) ) goto type_error;
                array_setindex( vm, array, (size_t)(int64_t)as_number( v ), r[ op.r ] );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                table_object* table = (table_object*)as_object( u );
                table_setindex( vm, table, v, r[ op.r ] );
                break;
            }
        }
        goto type_error;
    }

    case OP_SET_INDEXI:
    {
        value u = r[ op.a ];
        if ( is_object( u ) )
        {
            type_code type = header( as_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                array_object* array = (array_object*)as_object( u );
                array_setindex( vm, array, op.b, r[ op.r ] );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                table_object* table = (table_object*)as_object( u );
                table_setindex( vm, table, number_value( op.b ), r[ op.r ] );
                break;
            }
        }
        goto type_error;
    }

    case OP_NEW_ENV:
    {
        r[ op.r ] = object_value( vslots_new( vm, op.c ) );
        break;
    }

    case OP_GET_VARENV:
    {
        vslots_object* varenv = (vslots_object*)as_object( r[ op.a ] );
        r[ op.r ] = read( varenv->slots[ op.b ] );
        break;
    }

    case OP_SET_VARENV:
    {
        vslots_object* varenv = (vslots_object*)as_object( r[ op.a ] );
        write( vm, varenv->slots[ op.b ], r[ op.r ] );
        break;
    }

    case OP_GET_OUTENV:
    {
        vslots_object* outenv = read( function->outenvs[ op.a ] );
        r[ op.r ] = read( outenv->slots[ op.b ] );
        break;
    }

    case OP_SET_OUTENV:
    {
        vslots_object* outenv = read( function->outenvs[ op.a ] );
        write( vm, outenv->slots[ op.b ], r[ op.r ] );
        break;
    }

    case OP_NEW_OBJECT:
    {
        value u = r[ op.a ];
        lookup_object* prototype;
        if ( is_null( u ) )
        {
            prototype = vm->prototypes[ LOOKUP_OBJECT ];
        }
        else if ( is_object( u ) && header( as_object( u ) )->type == LOOKUP_OBJECT )
        {
            prototype = (lookup_object*)as_object( u );
        }
        else
        {
            goto type_error;
        }
        r[ op.r ] = object_value( lookup_new( vm, prototype ) );
        break;
    }

    case OP_NEW_ARRAY:
    {
        r[ op.r ] = object_value( array_new( vm, op.c ) );
        break;
    }

    case OP_NEW_TABLE:
    {
        r[ op.r ] = object_value( table_new( vm, op.c ) );
        break;
    }

    case OP_APPEND:
    {
        value w = r[ op.r ];
        if ( ! is_object( w ) || header( as_object( w ) )->type != ARRAY_OBJECT ) goto type_error;
        array_object* array = (array_object*)as_object( w );
        array_append( vm, array, r[ op.a ] );
        break;
    }

    case OP_CALL:
    case OP_CALLR:
    case OP_YCALL:
    case OP_YIELD:
    case OP_RETURN:
    case OP_VARARG:
    case OP_UNPACK:
    case OP_EXTEND:

    case OP_GENERATE:
    {
        /*
            | O | r | a | b |
            g : [ a ]
            if g is array:
                [ r + 0 ] = array
                [ r + 1 ] = 0
            if g is table:
                [ r + 0 ] = table
                [ r + 1 ] = index of first nonempty slot
            if g is cothread:
                [ r + 0 ] = cothread
            if g is string:
                [ r + 0 ] = string
                [ r + 1 ] = 0
        */
        value u = r[ op.a ];
        r[ op.r + 0 ] = u;
        if ( is_object( u ) )
        {
            type_code type = header( as_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                r[ op.r + 1 ] = { 0 };
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                r[ op.r + 1 ] = { (uint64_t)table_iterate( vm, (table_object*)as_object( u ) };
                break;
            }
            else if ( type == COTHREAD_OBJECT )
            {
                break;
            }
        }
        else if ( is_string( u ) )
        {
            r[ op.r + 1 ] = { 0 };
            break;
        }
        goto type_error;
    }

    case OP_FOR_EACH:
    {
        /*
            | O | r | a | b | J | - |   j   |
            g : [ a + 0 ]
            i : [ a + 1 ]
            if g is array:
                if i >= #array then jump
                [ r + 0 ] = g[ i ]
                [ r + 1 ] = i
                i += 1
            if g is table:
                if i >= table-slots then jump
                [ r + 0 ] = g[ i ].key
                [ r + 1 ] = g[ i ].value
                i = index of next nonempty slot
            if g is cothread:
                resume cothread.
                if cothread is finished, jump
                [ r ... ] = cothread results
            if g is string
                if i >= #string then jump
                [ r + 0 ] = g[ i ]
                [ r + 1 ] = i
                i += 1
        */

        break;
    }


    case OP_FOR_STEP:
    {
        /*
            | O | r | a | - | J | - |   j   |
            i     : number( [ a + 0 ] )
            limit : number( [ a + 1 ] )
            step  : number( [ a + 2 ] )
            [ r ] = i
            if step >= 0.0
                if i >= limit then jump
            else
                if i <= limit then jump
            i += step
        */

        value v0 = r[ op.a + 0 ];
        value v1 = r[ op.a + 1 ];
        value v2 = r[ op.a + 2 ];
        if ( ! is_number( v0 ) ) goto type_error;
        if ( ! is_number( v1 ) ) goto type_error;
        if ( ! is_number( v2 ) ) goto type_error;
        double i = as_number( v0 );
        double limit = as_number( v1 );
        double step = as_number( v2 );
        struct op jop = ops[ ip++ ];
        if ( step >= 0.0 ? i < limit : i > limit )
        {
            r[ op.r ] = number_value( i );
            r[ op.a ] = number_value( i + step );
        }
        else
        {
            ip += jop.j;
        }
        break;
    }

    case OP_SUPER:
    {
        // TODO.
        break;
    }

    case OP_THROW:
    {
        // TODO.
        throw std::exception();
    }

    case OP_FUNCTION:
    {
        program_object* program = read( read( function->program )->functions[ op.c ] );
        function_object* closure = function_new( vm, program );
        r[ op.r ] = object_value( closure );
        while ( true )
        {
            struct op vop = ops[ ip ];
            if ( vop.opcode == OP_F_VARENV )
            {
                assert( vop.r == op.r );
                winit( closure->outenvs[ op.a ], (vslots_object*)as_object( r[ op.b ] ) );
            }
            else if ( vop.opcode == OP_F_OUTENV )
            {
                assert( vop.r == op.r );
                winit( closure->outenvs[ op.a ], read( function->outenvs[ op.b ] ) );
            }
            else
            {
                break;
            }
            ++ip;
        }
    }

    case OP_F_VARENV:
    case OP_F_OUTENV:
        assert( ! "orphan environment op" );
        break;
    }

    }

type_error:
    throw std::exception();
}

}

