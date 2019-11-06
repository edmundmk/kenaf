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

static inline bool value_test( value u )
{
    // All values test true except null, false, -0.0, and +0.0.
    return u.v > 1 && u.v != box_number( +0.0 ).v && u.v != box_number( -0.0 ).v;
}

static inline bool string_equal( string_object* us, string_object* vs )
{
    return us == vs || ( us->size == vs->size && memcmp( us->text, vs->text, us->size ) == 0 );
}

static inline int string_compare( string_object* us, string_object* vs )
{
    if ( us == vs ) return 0;
    size_t size = std::min( us->size, vs->size );
    return memcmp( us->text, vs->text, size + 1 );
}

static lookup_object* keyer_of( vm_context* vm, value u )
{
    if ( box_is_number( u ) )
    {
        return vm->prototypes[ NUMBER_OBJECT ];
    }
    else if ( box_is_string( u ) )
    {
        return vm->prototypes[ STRING_OBJECT ];
    }
    else if ( box_is_object( u ) )
    {
        type_code type = header( unbox_object( u ) )->type;
        if ( type == LOOKUP_OBJECT )
        {
            return (lookup_object*)unbox_object( u );
        }
        else
        {
            return vm->prototypes[ type ];
        }
    }
    else if ( box_is_bool( u ) )
    {
        return vm->prototypes[ BOOL_OBJECT ];
    }
    else
    {
        return vm->prototypes[ NULL_OBJECT ];
    }
}

void vm_execute( vm_context* vm, vm_exstate state )
{
    function_object* function = state.function;
    const op* ops = read( function->program )->ops;
    ref_value* k = read( function->program )->constants;
    key_selector* s = read( function->program )->selectors;

    value* r = state.r;
    unsigned ip = state.ip;
    unsigned xp = state.xp;
    struct op op;

    while ( true )
    {

    double n;
    string_object* us;
    string_object* vs;

    op = ops[ ip++ ];
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

    case OP_NEG:
    {
        value u = r[ op.a ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        r[ op.r ] = box_number( -unbox_number( u ) );
        break;
    }

    case OP_POS:
    {
        value u = r[ op.a ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        r[ op.r ] = box_number( +unbox_number( u ) );
        break;
    }

    op_add:
    {
        value u = r[ op.a ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        r[ op.r ] = box_number( unbox_number( u ) + n );
        break;
    }

    case OP_ADD:
    {
        value v = r[ op.b ];
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        n = unbox_number( v );
        goto op_add;
    }

    case OP_ADDN:
    {
        n = unbox_number( read( k[ op.b ] ) );
        goto op_add;
    }

    op_sub:
    {
        value u = r[ op.a ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        r[ op.r ] = box_number( n - unbox_number( u ) );
        break;
    }

    case OP_SUB:
    {
        value v = r[ op.b ];
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        n = unbox_number( v );
        goto op_sub;
    }

    case OP_SUBN:
    {
        n = unbox_number( read( k[ op.b ] ) );
        goto op_sub;
    }

    op_mul:
    {
        value u = r[ op.a ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        r[ op.r ] = box_number( unbox_number( u ) * n );
        break;
    }

    case OP_MUL:
    {
        value v = r[ op.b ];
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        n = unbox_number( v );
        goto op_mul;
    }

    case OP_MULN:
    {
        n = unbox_number( read( k[ op.b ] ) );
        goto op_mul;
    }

    case OP_DIV:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        r[ op.r ] = box_number( unbox_number( u ) / unbox_number( v ) );
        break;
    }

    case OP_INTDIV:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        r[ op.r ] = box_number( ifloordiv( unbox_number( u ), unbox_number( v ) ) );
        break;
    }

    case OP_MOD:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        r[ op.r ] = box_number( ifloormod( unbox_number( u ), unbox_number( v ) ) );
        break;
    }

    case OP_NOT:
    {
        value u = r[ op.a ];
        r[ op.r ] = value_test( u ) ? boxed_false : boxed_true;
        break;
    }

    case OP_JMP:
    {
        ip += op.j;
        break;
    }

    case OP_JT:
    {
        if ( value_test( r[ op.r ] ) )
        {
            ip += op.j;
        }
        break;
    }

    case OP_JF:
    {
        if ( ! value_test( r[ op.r ] ) )
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
        if ( box_is_number( u ) )
        {
            test = box_is_number( v ) && unbox_number( u ) == unbox_number( v );
        }
        else if ( u.v == v.v )
        {
            test = true;
        }
        else if ( box_is_string( u ) )
        {
            test = box_is_string( v ) && string_equal( unbox_string( u ), unbox_string( v ) );
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
        bool test = box_is_number( u ) && unbox_number( u ) == unbox_number( read( k[ op.b ] ) );
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
        bool test = box_is_string( u ) && string_equal( unbox_string( u ), unbox_string( read( k[ op.b ] ) ) );
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
        if ( box_is_number( u ) )
        {
            if ( ! box_is_number( v ) ) goto type_error_b_number;
            test = unbox_number( u ) < unbox_number( v );
        }
        else if ( box_is_string( u ) )
        {
            if ( ! box_is_string( v ) ) goto type_error_b_string;
            test = string_compare( unbox_string( u ), unbox_string( v ) ) < 0;
        }
        else
        {
            goto type_error_a_number_or_string;
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
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        bool test = unbox_number( u ) < unbox_number( read( k[ op.b ] ) );
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
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        bool test = unbox_number( u ) > unbox_number( read( k[ op.b ] ) );
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
        if ( box_is_number( u ) )
        {
            if ( ! box_is_number( v ) ) goto type_error_a_number;
            test = unbox_number( u ) <= unbox_number( v );
        }
        else if ( box_is_string( u ) )
        {
            if ( ! box_is_string( v ) ) goto type_error_b_string;
            test = string_compare( unbox_string( u ), unbox_string( v ) ) <= 0;
        }
        else
        {
            goto type_error_a_number_or_string;
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
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        bool test = unbox_number( u ) <= unbox_number( read( k[ op.b ] ) );
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
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        bool test = unbox_number( u ) >= unbox_number( read( k[ op.b ] ) );
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
        if ( ! box_is_object_type( u, LOOKUP_OBJECT ) ) goto type_error_a_lookup;
        lookup_setkey( vm, (lookup_object*)unbox_object( u ), read( ks->key ), &ks->sel, r[ op.r ] );
        break;
    }

    case OP_GET_INDEX:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( box_is_object( u ) )
        {
            type_code type = header( unbox_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                array_object* array = (array_object*)unbox_object( u );
                if ( ! box_is_number( v ) ) goto type_error_b_number;
                r[ op.r ] = array_getindex( vm, array, (size_t)(int64_t)unbox_number( v ) );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                table_object* table = (table_object*)unbox_object( u );
                r[ op.r ] = table_getindex( vm, table, v );
                break;
            }
        }
        else if ( box_is_string( u ) )
        {
            string_object* string = unbox_string( u );
            r[ op.r ] = box_string( string_getindex( vm, string, (size_t)(int64_t)unbox_number( v ) ) );
            break;
        }
        goto type_error_a_indexable;
    }

    case OP_GET_INDEXI:
    {
        value u = r[ op.a ];
        if ( box_is_object( u ) )
        {
            type_code type = header( unbox_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                array_object* array = (array_object*)unbox_object( u );
                r[ op.r ] = array_getindex( vm, array, op.b );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                table_object* table = (table_object*)unbox_object( u );
                r[ op.r ] = table_getindex( vm, table, box_number( op.b ) );
                break;
            }
        }
        else if ( box_is_string( u ) )
        {
            string_object* string = unbox_string( u );
            r[ op.r ] = box_string( string_getindex( vm, string, op.b ) );
            break;
        }
        goto type_error_a_indexable;
    }

    case OP_SET_INDEX:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( box_is_object( u ) )
        {
            type_code type = header( unbox_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                array_object* array = (array_object*)unbox_object( u );
                if ( ! box_is_number( v ) ) goto type_error_b_number;
                array_setindex( vm, array, (size_t)(int64_t)unbox_number( v ), r[ op.r ] );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                table_object* table = (table_object*)unbox_object( u );
                table_setindex( vm, table, v, r[ op.r ] );
                break;
            }
        }
        goto type_error_a_indexable;
    }

    case OP_SET_INDEXI:
    {
        value u = r[ op.a ];
        if ( box_is_object( u ) )
        {
            type_code type = header( unbox_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                array_object* array = (array_object*)unbox_object( u );
                array_setindex( vm, array, op.b, r[ op.r ] );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                table_object* table = (table_object*)unbox_object( u );
                table_setindex( vm, table, box_number( op.b ), r[ op.r ] );
                break;
            }
        }
        goto type_error_a_indexable;
    }

    case OP_NEW_ENV:
    {
        r[ op.r ] = box_object( vslots_new( vm, op.c ) );
        break;
    }

    case OP_GET_VARENV:
    {
        vslots_object* varenv = (vslots_object*)unbox_object( r[ op.a ] );
        r[ op.r ] = read( varenv->slots[ op.b ] );
        break;
    }

    case OP_SET_VARENV:
    {
        vslots_object* varenv = (vslots_object*)unbox_object( r[ op.a ] );
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

    case OP_FUNCTION:
    {
        program_object* program = read( read( function->program )->functions[ op.c ] );
        function_object* closure = function_new( vm, program );
        unsigned rp = op.r;
        while ( true )
        {
            op = ops[ ip ];
            if ( op.opcode == OP_F_METHOD )
            {
                assert( op.r == rp );
                value omethod = r[ op.a ];
                if ( ! box_is_object_type( omethod, LOOKUP_OBJECT ) ) goto type_error_a_lookup;
                winit( closure->omethod, (lookup_object*)unbox_object( omethod ) );
            }
            else if ( op.opcode == OP_F_VARENV )
            {
                assert( op.r == rp );
                winit( closure->outenvs[ op.a ], (vslots_object*)unbox_object( r[ op.b ] ) );
            }
            else if ( op.opcode == OP_F_OUTENV )
            {
                assert( op.r == rp );
                winit( closure->outenvs[ op.a ], read( function->outenvs[ op.b ] ) );
            }
            else
            {
                break;
            }
            ++ip;
        }
        r[ rp ] = box_object( closure );
        break;
    }

    case OP_NEW_OBJECT:
    {
        value u = r[ op.a ];
        lookup_object* prototype;
        if ( box_is_object_type( u, LOOKUP_OBJECT ) )
        {
            prototype = (lookup_object*)unbox_object( u );
        }
        else if ( box_is_null( u ) )
        {
            prototype = vm->prototypes[ LOOKUP_OBJECT ];
        }
        else
        {
            goto type_error_a_lookup;
        }
        r[ op.r ] = box_object( lookup_new( vm, prototype ) );
        break;
    }

    case OP_NEW_ARRAY:
    {
        r[ op.r ] = box_object( array_new( vm, op.c ) );
        break;
    }

    case OP_NEW_TABLE:
    {
        r[ op.r ] = box_object( table_new( vm, op.c ) );
        break;
    }

    case OP_APPEND:
    {
        value u = r[ op.a ];
        if ( ! box_is_object_type( u, ARRAY_OBJECT ) ) goto type_error_a_array;
        array_object* array = (array_object*)unbox_object( u );
        array_append( vm, array, r[ op.b ] );
        break;
    }

    case OP_CALL:
    case OP_CALLR:
    case OP_YCALL:
    {
        /*
            Object types that you can call:
                Lookup Objects  Get self method and pass a new object to it plus parameters.
                Functions       Construct call frame for function, continue.
                Generators      Create cothread for generator, assign initial parameters.
                Cothreads       Push cothread on stack, resume yielded cothread.
        */

        // First determine rp:xp for arguments.
        unsigned rp = op.r;
        if ( op.a != OP_STACK_MARK )
        {
            r = vm_resize_stack( vm, xp = op.a );
        }

        // Store ip, xr:xb in current stack frame.
        vm_stack_frame* stack_frame = vm_active_frame( vm );
        stack_frame->ip = ip;
        stack_frame->resume = RESUME_CALL;
        stack_frame->xr = op.r;
        if ( op.opcode == OP_CALLR )
        {
            stack_frame->xb = op.r + 1;
            stack_frame->rr = op.b;
        }
        else
        {
            stack_frame->xb = op.b;
            stack_frame->rr = op.r;
        }

        // Find called object.
        value w = r[ op.r ];
        if ( ! box_is_object( w ) ) goto type_error_r_callable;
        type_code type = header( unbox_object( w ) )->type;

        if ( type == LOOKUP_OBJECT )
        {
            // Lookup w.self.
            lookup_object* class_object = (lookup_object*)unbox_object( w );
            value method = lookup_getkey( vm, class_object, vm->self_key, &vm->self_sel );

            // Construct new object.
            lookup_object* self = lookup_new( vm, class_object );

            // Rearrange stack.
            r = vm_resize_stack( vm, xp + 2 );
            memcpy( r + rp + 2, r + rp, sizeof( value ) * ( xp - rp ) );
            r[ rp + 0 ] = box_object( self );
            r[ rp + 1 ] = method;
            r[ rp + 2 ] = box_object( self );
            op.r += 1;
            rp += 1;
            xp += 2;

            // Return needs to know about the self parameter.
            stack_frame->resume = RESUME_CONSTRUCT;

            // Continue adjusted call.
            w = method;
            if ( ! box_is_object( w ) ) goto type_error_r_callable;
            type = header( unbox_object( w ) )->type;
        }

        vm_exstate state;
        if ( type == FUNCTION_OBJECT )
        {
            function_object* call_function = (function_object*)unbox_object( w );
            program_object* call_program = read( call_function->program );
            if ( op.opcode == OP_YCALL || ( call_program->code_flags & CODE_GENERATOR ) == 0 )
            {
                state = vm_call( vm, call_function, rp, xp );
            }
            else
            {
                state = vm_call_generator( vm, call_function, rp, xp );
            }
        }
        else if ( type == NATIVE_FUNCTION_OBJECT )
        {
            native_function_object* call_function = (native_function_object*)unbox_object( w );
            state = vm_call_native( vm, call_function, rp, xp );
        }
        else if ( type == COTHREAD_OBJECT )
        {
            // Resume yielded cothread.
            cothread_object* cothread = (cothread_object*)unbox_object( w );
            state = vm_call_cothread( vm, cothread, rp, xp );
        }
        else
        {
            goto type_error_r_callable;
        }

        function = state.function;
        ops = read( function->program )->ops;
        k = read( function->program )->constants;
        s = read( function->program )->selectors;
        r = state.r;
        ip = state.ip;
        xp = state.xp;
        break;
    }

    case OP_YIELD:
    {
        // First determine rp:xp for arguments.
        unsigned rp = op.r;
        if ( op.a != OP_STACK_MARK )
        {
            r = vm_resize_stack( vm, xp = op.a );
        }

        // Store ip, xr:xb in current stack frame.
        vm_stack_frame* stack_frame = vm_active_frame( vm );
        stack_frame->ip = ip;
        stack_frame->resume = RESUME_YIELD;
        stack_frame->xr = op.r;
        stack_frame->xb = op.b;
        stack_frame->rr = op.r;

        // Yield.
        vm_exstate state = vm_yield( vm, rp, xp );

        if ( ! state.function )
        {
            return;
        }

        function = state.function;
        ops = read( function->program )->ops;
        k = read( function->program )->constants;
        s = read( function->program )->selectors;
        r = state.r;
        ip = state.ip;
        xp = state.xp;
        break;
    }

    case OP_RETURN:
    {
        // Determine rp:xp for arguments.
        unsigned rp = op.r;
        if ( op.a != OP_STACK_MARK )
        {
            r = vm_resize_stack( vm, xp = op.a );
        }

        // Return.
        vm_exstate state = vm_return( vm, rp, xp );

        if ( ! state.function )
        {
            return;
        }

        function = state.function;
        ops = read( function->program )->ops;
        k = read( function->program )->constants;
        s = read( function->program )->selectors;
        r = state.r;
        ip = state.ip;
        xp = state.xp;
        break;
    }

    case OP_VARARG:
    {
        // Unpack varargs into r:b.
        vm_stack_frame* stack_frame = vm_active_frame( vm );
        unsigned rp = op.r;
        xp = op.b != OP_STACK_MARK ? op.b : rp + stack_frame->fp - stack_frame->bp;
        r = vm_resize_stack( vm, xp );
        value* stack = vm_entire_stack( vm );
        size_t ap = stack_frame->bp;
        size_t fp = stack_frame->fp;
        while ( rp < xp )
        {
            r[ rp++ ] = ap < fp ? stack[ ap++ ] : boxed_null;
        }
        break;
    }

    case OP_UNPACK:
    {
        // Unpack array elements from a into r:b.
        value u = r[ op.a ];
        if ( ! box_is_object_type( u, ARRAY_OBJECT ) ) goto type_error_a_array;
        array_object* array = (array_object*)unbox_object( u );
        unsigned rp = op.r;
        xp = op.b != OP_STACK_MARK ? op.b : rp + array->length;
        r = vm_resize_stack( vm, xp );
        size_t i = 0;
        while ( rp < xp )
        {
            r[ rp++ ] = i < array->length ? array_getindex( vm, array, i++ ) : boxed_null;
        }
        break;
    }

    case OP_EXTEND:
    {
        // Extend array in b with values in r:a.
        value v = r[ op.b ];
        if ( ! box_is_object_type( v, ARRAY_OBJECT ) ) goto type_error_b_array;
        array_object* array = (array_object*)unbox_object( v );
        unsigned rp = op.r;
        if ( op.a != OP_STACK_MARK )
        {
            r = vm_resize_stack( vm, xp = op.a );
        }
        assert( rp <= xp );
        array_extend( vm, array, r + rp, xp - rp );
        break;
    }

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
        if ( box_is_object( u ) )
        {
            type_code type = header( unbox_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                r[ op.r + 1 ] = box_index( 0 );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                uint64_t index = table_iterate( vm, (table_object*)unbox_object( u ) );
                r[ op.r + 1 ] = box_index( index );
                break;
            }
            else if ( type == COTHREAD_OBJECT )
            {
                break;
            }
        }
        else if ( box_is_string( u ) )
        {
            r[ op.r + 1 ] = box_index( 0 );
            break;
        }
        goto type_error_a_iterable;
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
        value g = r[ op.a + 0 ];
        struct op jop = ops[ ip++ ];
        unsigned rp = op.r;
        if ( box_is_object( g ) )
        {
            type_code type = header( unbox_object( g ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                array_object* array = (array_object*)unbox_object( g );
                size_t i = unbox_index( r[ op.a + 1 ] );
                if ( i < array->length )
                {
                    xp = op.b != OP_STACK_MARK ? op.b : rp + 2;
                    r = vm_resize_stack( vm, xp );
                    if ( rp < xp ) r[ rp++ ] = read( read( array->aslots )->slots[ i++ ] );
                    if ( rp < xp ) r[ rp++ ] = box_number( i );
                    while ( rp < xp )
                    {
                        r[ rp++ ] = boxed_null;
                    }
                    r[ op.a + 1 ] = box_index( i );
                }
                else
                {
                    ip += jop.j;
                }
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                table_object* table = (table_object*)unbox_object( g );
                size_t i = unbox_index( r[ op.a + 1 ] );
                table_keyval keyval;
                if ( table_next( vm, table, &i, &keyval ) )
                {
                    xp = op.b != OP_STACK_MARK ? op.b : rp + 2;
                    r = vm_resize_stack( vm, xp );
                    if ( rp < xp ) r[ rp++ ] = keyval.k;
                    if ( rp < xp ) r[ rp++ ] = keyval.v;
                    while ( rp < xp )
                    {
                        r[ rp++ ] = boxed_null;
                    }
                    r[ op.a + 1 ] = box_index( i );
                }
                else
                {
                    ip += jop.j;
                }
                break;
            }
            else if ( type == COTHREAD_OBJECT )
            {
                // Resume generator with no arguments.
                cothread_object* cothread = (cothread_object*)unbox_object( g );

                vm_stack_frame* stack_frame = vm_active_frame( vm );
                stack_frame->ip = ip;
                stack_frame->resume = RESUME_FOR_EACH;
                stack_frame->xr = op.r;
                stack_frame->xb = op.b;
                stack_frame->rr = op.r;

                r[ rp ] = g;
                vm_exstate state = vm_call_cothread( vm, cothread, rp, rp + 1 );

                function = state.function;
                ops = read( function->program )->ops;
                k = read( function->program )->constants;
                s = read( function->program )->selectors;
                r = state.r;
                ip = state.ip;
                xp = state.xp;
                break;
            }
        }
        else if ( box_is_string( g ) )
        {
            string_object* string = unbox_string( g );
            size_t i = unbox_index( r[ op.a + 1 ] );
            if ( i < string->size )
            {
                xp = op.b != OP_STACK_MARK ? op.b : rp + 2;
                r = vm_resize_stack( vm, xp );
                if ( rp < xp ) r[ rp++ ] = box_string( string_getindex( vm, string, i++ ) );
                while ( rp < xp )
                {
                    r[ rp++ ] = boxed_null;
                    r[ op.a + 1 ] = box_index( i );
                }
            }
            else
            {
                ip += jop.j;
            }
            break;
        }
        goto type_error_a_iterable;
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
        if ( ! box_is_number( v0 ) ) goto type_error_a_number;
        if ( ! box_is_number( v1 ) ) goto type_error_a1_number;
        if ( ! box_is_number( v2 ) ) goto type_error_a2_number;
        double i = unbox_number( v0 );
        double limit = unbox_number( v1 );
        double step = unbox_number( v2 );
        struct op jop = ops[ ip++ ];
        if ( step >= 0.0 ? i < limit : i > limit )
        {
            r[ op.r ] = box_number( i );
            r[ op.a ] = box_number( i + step );
        }
        else
        {
            ip += jop.j;
        }
        break;
    }

    op_concat:
    {
        string_object* s = string_new( vm, nullptr, us->size + vs->size );
        memcpy( s->text, us->text, us->size );
        memcpy( s->text + us->size, vs->text, vs->size );
        r[ op.r ] = box_string( s );
        break;
    }

    op_concatu:
    {
        value u = r[ op.a ];
        if ( ! box_is_string( u ) ) goto type_error_a_string;
        us = unbox_string( u );
        goto op_concat;
    }

    case OP_CONCAT:
    {
        value v = r[ op.b ];
        if ( ! box_is_string( v ) ) goto type_error_b_string;
        vs = unbox_string( v );
        goto op_concatu;
    }

    case OP_CONCATS:
    {
        vs = unbox_string( read( k[ op.b ] ) );
        goto op_concatu;
    }

    case OP_RCONCATS:
    {
        value v = r[ op.a ];
        us = unbox_string( read( k[ op.b ] ) );
        if ( ! box_is_string( v ) ) goto type_error_a_string;
        vs = unbox_string( v );
        goto op_concat;
    }

    case OP_BITNOT:
    {
        value u = r[ op.a ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        r[ op.r ] = box_number( ibitnot( unbox_number( u ) ) );
        break;
    }

    case OP_LSHIFT:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        r[ op.r ] = box_number( ilshift( unbox_number( u ), unbox_number( v ) ) );
        break;
    }

    case OP_RSHIFT:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        r[ op.r ] = box_number( irshift( unbox_number( u ), unbox_number( v ) ) );
        break;
    }

    case OP_ASHIFT:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        r[ op.r ] = box_number( iashift( unbox_number( u ), unbox_number( v ) ) );
        break;
    }

    case OP_BITAND:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        r[ op.r ] = box_number( ibitand( unbox_number( u ), unbox_number( v ) ) );
        break;
    }

    case OP_BITXOR:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        r[ op.r ] = box_number( ibitxor( unbox_number( u ), unbox_number( v ) ) );
        break;
    }

    case OP_BITOR:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        if ( ! box_is_number( u ) ) goto type_error_a_number;
        if ( ! box_is_number( v ) ) goto type_error_b_number;
        r[ op.r ] = box_number( ibitor( unbox_number( u ), unbox_number( v ) ) );
        break;
    }

    case OP_LEN:
    {
        value u = r[ op.a ];
        if ( box_is_object( u ) )
        {
            type_code type = header( unbox_object( u ) )->type;
            if ( type == ARRAY_OBJECT )
            {
                r[ op.r ] = box_number( ( (array_object*)unbox_object( u ) )->length );
                break;
            }
            else if ( type == TABLE_OBJECT )
            {
                r[ op.r ] = box_number( ( (table_object*)unbox_object( u ) )->length );
                break;
            }
        }
        else if ( box_is_string( u ) )
        {
            r[ op.r ] = box_number( unbox_string( u )->size );
            break;
        }
        goto type_error_a_indexable;
    }

    case OP_IS:
    {
        value u = r[ op.a ];
        value v = r[ op.b ];
        bool test = false;
        if ( box_is_number( v ) )
        {
            test = box_is_number( u ) && unbox_number( u ) == unbox_number( v );
        }
        else if ( u.v == v.v )
        {
            test = true;
        }
        else if ( box_is_string( v ) )
        {
            test = box_is_string( u ) && string_equal( unbox_string( u ), unbox_string( v ) );
        }
        else if ( box_is_object( v ) )
        {
            type_code type = header( unbox_object( v ) )->type;
            if ( type == LOOKUP_OBJECT )
            {
                lookup_object* vo = (lookup_object*)unbox_object( v );
                lookup_object* uo = keyer_of( vm, u );
                while ( uo )
                {
                    if ( uo == vo )
                    {
                        test = true;
                        break;
                    }
                    uo = lookup_prototype( vm, uo );
                }
            }
        }
        r[ op.r ] = test ? boxed_true : boxed_false;
        break;
    }

    case OP_SUPER:
    {
        lookup_object* omethod = read( function->omethod );
        r[ op.r ] = box_object( lookup_prototype( vm, omethod ) );
        break;
    }

    case OP_THROW:
    {
        vm_throw( r[ op.a ] );
        return;
    }

    case OP_F_METHOD:
    case OP_F_VARENV:
    case OP_F_OUTENV:
        assert( ! "orphan environment op" );
        break;
    }

    }

type_error_a_number:
    vm_type_error( r[ op.a ], "a number" );
    return;

type_error_a_string:
    vm_type_error( r[ op.a ], "a string" );
    return;

type_error_a_number_or_string:
    vm_type_error( r[ op.a ], "a number or string" );
    return;

type_error_a_lookup:
    vm_type_error( r[ op.a ], "a lookup object" );
    return;

type_error_a_array:
    vm_type_error( r[ op.a ], "an array" );
    return;

type_error_a_indexable:
    vm_type_error( r[ op.a ], "indexable" );
    return;

type_error_a_iterable:
    vm_type_error( r[ op.a ], "iterable" );
    return;

type_error_b_number:
    vm_type_error( r[ op.b ], "a number" );
    return;

type_error_b_string:
    vm_type_error( r[ op.b ], "a string" );
    return;

type_error_b_array:
    vm_type_error( r[ op.b ], "an array" );
    return;

type_error_r_callable:
    vm_type_error( r[ op.r ], "callable" );
    return;

type_error_a1_number:
    vm_type_error( r[ op.a + 1 ], "a number" );
    return;

type_error_a2_number:
    vm_type_error( r[ op.a + 2 ], "a number" );
    return;
}

}

