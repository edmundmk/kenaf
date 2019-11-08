//
//  call_stack.cpp
//
//  Created by Edmund Kapusniak on 29/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "call_stack.h"
#include "kenaf/errors.h"
#include "../objects/cothread_object.h"

namespace kf
{

static xstate stack_return( vmachine* vm, cothread_object* cothread, const stack_frame* stack_frame, unsigned return_fp, unsigned rp, unsigned xp );
static xstate yield_return( vmachine* vm, cothread_object* cothread, const stack_frame* stack_frame, const value* yield_r, unsigned rp, unsigned xp );

stack_frame* active_frame( vmachine* vm )
{
    return &vm->cothreads->back()->stack_frames.back();
}

value* resize_stack( vmachine* vm, unsigned xp )
{
    cothread_object* cothread = vm->cothreads->back();
    return resize_stack( vm->cothreads->back(), cothread->stack_frames.back().fp, xp );
}

value* resize_stack( cothread_object* cothread, unsigned fp, unsigned xp )
{
    // xp is relative to current frame pointer.
    cothread->xp = fp + xp;

    // Increase stack size.
    if ( cothread->xp > cothread->stack.size() )
    {
        size_t size = ( cothread->xp + 31u ) & ~(size_t)31u;
        cothread->stack.resize( size );
    }

    // Return possibly reallocated stack.
    return cothread->stack.data() + fp;
}

value* entire_stack( vmachine* vm )
{
    cothread_object* cothread = vm->cothreads->back();
    return cothread->stack.data();
}

xstate call_function( vmachine* vm, function_object* function, unsigned rp, unsigned xp )
{
    /*
        call rp:xp
    */

    assert( rp < xp );

    program_object* program = read( function->program );
    bool is_varargs = ( program->code_flags & CODE_VARARGS ) != 0;
    unsigned argument_count = xp - ( rp + 1 );
    if ( argument_count < program->param_count || ( argument_count > program->param_count && ! is_varargs ) )
    {
        throw argument_error( "incorrect argument count, expected %u, got %u", program->param_count, argument_count );
    }

    cothread_object* cothread = vm->cothreads->back();
    unsigned bp = cothread->stack_frames.back().fp + rp;
    cothread->stack_frames.push_back( { function, bp, bp, 0, RESUME_CALL, 0, 0, 0 } );
    stack_frame* stack_frame = &cothread->stack_frames.back();

    if ( is_varargs )
    {
        /*
            Arguments are in memory in this order:

                bp  ->  function
                        arg0
                        vararg0
                fp  ->  vararg1
                        vararg2
                xp  ->

            We want to reorder them so that they're in this order:

                bp  ->  vararg0
                        vararg1
                        vararg2
                fp  ->  function
                        arg0
                xp  ->
        */
        value* r = cothread->stack.data() + bp;
        unsigned total_count = xp - rp;
        unsigned split_count = program->param_count + 1;
        std::reverse( r, r + split_count );
        std::reverse( r + split_count, r + total_count );
        std::reverse( r, r + total_count );
        stack_frame->fp = bp + total_count - split_count;
    }

    value* r = resize_stack( cothread, stack_frame->fp, program->stack_size );
    return { stack_frame->function, r, stack_frame->ip, cothread->xp - stack_frame->fp };
}

xstate call_native( vmachine* vm, native_function_object* function, unsigned rp, unsigned xp )
{
    /*
        call native rp:xp -> rp:count
    */

    assert( rp < xp );

    bool is_varargs = ( function->code_flags & CODE_VARARGS ) != 0;
    unsigned argument_count = xp - ( rp + 1 );
    if ( argument_count < function->param_count || ( argument_count > function->param_count && ! is_varargs ) )
    {
        throw argument_error( "incorrect argument count, expected %u, got %u", function->param_count, argument_count );
    }

    cothread_object* cothread = vm->cothreads->back();
    size_t frame_count = cothread->stack_frames.size();
    unsigned bp = cothread->stack_frames.back().fp + rp;

    frame native_frame = { cothread, bp };
    value* arguments = cothread->stack.data() + bp + 1;
    size_t result_count;
    try
    {
        result_count = function->native( function->cookie, (frame*)&native_frame, arguments, argument_count );
    }
    catch ( script_error& e )
    {
        e.append_stack_trace( "[native]: %.*s", (int)function->name_size, function->name_text );
        throw;
    }

    assert( vm->cothreads->back() == cothread );
    assert( cothread->stack_frames.size() == frame_count );

    const stack_frame* stack_frame = &cothread->stack_frames.back();
    return stack_return( vm, cothread, stack_frame, bp, 0, result_count );
}

xstate call_generator( vmachine* vm, function_object* function, unsigned rp, unsigned xp )
{
    /*
        call generator rp:xp -> rp:rp+1 [generator]
    */

    assert( rp < xp );

    program_object* program = read( function->program );
    bool is_varargs = ( program->code_flags & CODE_VARARGS ) != 0;
    unsigned argument_count = xp - ( rp + 1 );
    if ( argument_count < program->param_count || ( argument_count > program->param_count && ! is_varargs ) )
    {
        throw argument_error( "incorrect argument count, expected %u, got %u", program->param_count, argument_count );
    }

    // Get current stack.
    cothread_object* caller_cothread = vm->cothreads->back();
    const stack_frame* caller_frame = &caller_cothread->stack_frames.back();
    unsigned caller_bp = caller_frame->fp + rp;
    value* caller_r = caller_cothread->stack.data() + caller_bp;

    // Create new cothread.
    cothread_object* generator_cothread = cothread_new( vm );
    generator_cothread->stack_frames.push_back( { function, 0, 0, 0, RESUME_YIELD, 0, 0, 0 } );
    stack_frame* generator_frame = &generator_cothread->stack_frames.back();

    /*
        Arguments are on caller's stack:

            rp  ->  function
                    arg0
                    vararg0
                    vararg1
                    vararg2
            xp ->

        Copy to generator's stacK:

            bp  ->  vararg0
                    vararg1
                    vararg2
            fp  ->  function
                    arg0
    */

    // Copy arguments to cothread's stack.
    unsigned stack_size = std::max< unsigned >( program->stack_size, 1 + argument_count );
    value* generator_r = resize_stack( generator_cothread, 0, stack_size );
    unsigned actual_count = 1 + program->param_count;
    unsigned vararg_count = xp - rp - actual_count;
    memcpy( generator_r, caller_r + actual_count, vararg_count * sizeof( value ) );
    memcpy( generator_r + vararg_count, caller_r, actual_count * sizeof( value ) );
    generator_frame->fp = vararg_count;

    // Return with the generator as the result.
    caller_r[ 0 ] = box_object( generator_cothread );
    return stack_return( vm, caller_cothread, caller_frame, caller_bp, 0, 1 );
}

xstate call_cothread( vmachine* vm, cothread_object* cothread, unsigned rp, unsigned xp )
{
    /*
        call cothread rp:xp, to new cothread
    */

    assert( rp < xp );
    rp += 1;

    // Cothread might have completed.
    if ( cothread->stack_frames.empty() )
    {
        throw cothread_error( "cothread is done" );
    }

    // Get current stack.
    cothread_object* caller_cothread = vm->cothreads->back();
    value* caller_r = caller_cothread->stack.data() + caller_cothread->stack_frames.back().fp;

    // Get stack frame we are resuming into.
    const stack_frame* stack_frame = &cothread->stack_frames.back();
    assert( stack_frame->resume == RESUME_YIELD );
    assert( stack_frame->rr == stack_frame->xr );

    // Work out place in stack to copy to.
    unsigned xr = stack_frame->xr;
    unsigned xb = stack_frame->xb != OP_STACK_MARK ? stack_frame->xb : xr + ( xp - rp );
    value* r = resize_stack( cothread, stack_frame->fp, xb );

    // Copy parameters into cothread.
    while ( xr < xb )
    {
        r[ xr++ ] = rp < xp ? caller_r[ rp++ ] : boxed_null;
    }

    // Continue with new cothread.
    vm->cothreads->push_back( cothread );
    return { stack_frame->function, r, stack_frame->ip, cothread->xp - stack_frame->fp };
}

xstate call_return( vmachine* vm, unsigned rp, unsigned xp )
{
    assert( rp <= xp );

    // Get current stack.
    cothread_object* cothread = vm->cothreads->back();
    stack_frame return_frame = cothread->stack_frames.back();
    cothread->stack_frames.pop_back();

    if ( cothread->stack_frames.size() )
    {
        // Normal return.
        const stack_frame* stack_frame = &cothread->stack_frames.back();
        return stack_return( vm, cothread, stack_frame, return_frame.fp, rp, xp );
    }
    else
    {
        // Complete cothread.
        cothread_object* yield_cothread = cothread;
        vm->cothreads->pop_back();

        assert( ! vm->cothreads->empty() );
        cothread = vm->cothreads->back();
        const stack_frame* stack_frame = &cothread->stack_frames.back();

        if ( stack_frame->resume != RESUME_FOR_EACH )
        {
            // Return across cothreads.
            const value* yield_r = yield_cothread->stack.data() + return_frame.fp;
            return yield_return( vm, cothread, stack_frame, yield_r, rp, xp );
        }
        else
        {
            // No results, end iteration by jumping.
            value* r = resize_stack( cothread, stack_frame->fp, rp );
            return { stack_frame->function, r, stack_frame->ip - 1, cothread->xp - stack_frame->fp };
        }
    }
}

xstate call_yield( vmachine* vm, unsigned rp, unsigned xp )
{
    assert( rp <= xp );

    // Suspend cothread.
    cothread_object* yield_cothread = vm->cothreads->back();
    value* yield_r = yield_cothread->stack.data() + yield_cothread->stack_frames.back().fp;
    vm->cothreads->pop_back();

    // Get cothread we are yielding into.
    assert( ! vm->cothreads->empty() );
    cothread_object* cothread = vm->cothreads->back();
    const stack_frame* stack_frame = &cothread->stack_frames.back();

    // Return across cothreads.
    return yield_return( vm, cothread, stack_frame, yield_r, rp, xp );
}

static xstate stack_return( vmachine* vm, cothread_object* cothread, const stack_frame* stack_frame, unsigned return_fp, unsigned rp, unsigned xp )
{
    size_t result_count = rp - xp;
    unsigned xr = stack_frame->xr;
    unsigned xb = stack_frame->xb != OP_STACK_MARK ? stack_frame->xb : xr + result_count;

    // return_r with function we're returning from, r with function we're returning to.
    value* return_r = cothread->stack.data() + return_fp;
    value* r = cothread->stack.data() + stack_frame->fp;

    assert( r <= return_r );
    assert( r + xr <= return_r + rp );
    assert( stack_frame->fp + xb <= cothread->stack.size() );

    if ( stack_frame->resume == RESUME_CONSTRUCT && result_count == 0 )
    {
        xr += 1;
    }

    size_t value_count = std::min< size_t >( result_count, xb - xr );
    if ( r + xr < return_r + rp )
    {
        memmove( r + xr, return_r + rp, value_count + sizeof( value ) );
    }
    xr += value_count;

    while ( xr < xb )
    {
        r[ xr++ ] = boxed_null;
    }

    if ( stack_frame->rr != stack_frame->xr )
    {
        r[ stack_frame->rr ] = r[ stack_frame->xr ];
    }

    cothread->xp = stack_frame->fp + xb;
    return { stack_frame->function, r, stack_frame->ip, xb };
}

static xstate yield_return( vmachine* vm, cothread_object* cothread, const stack_frame* stack_frame, const value* yield_r, unsigned rp, unsigned xp )
{
    assert( rp <= xp );

    // Copy results.
    size_t result_count = rp - xp;
    unsigned xr = stack_frame->xr;
    unsigned xb = stack_frame->xb != OP_STACK_MARK ? stack_frame->xb : xr + result_count;
    value* r = resize_stack( cothread, stack_frame->fp, xb );

    if ( stack_frame->resume == RESUME_CONSTRUCT && result_count == 0 )
    {
        xr += 1;
    }

    while ( xr < xb )
    {
        r[ xr++ ] = rp < xp ? yield_r[ rp++ ] : boxed_null;
    }

    if ( stack_frame->rr != stack_frame->xr )
    {
        r[ stack_frame->rr ] = r[ stack_frame->xr ];
    }

    // Continue with yielded-to cothread.
    return { stack_frame->function, r, stack_frame->ip, cothread->xp - stack_frame->fp };
}

/*
    Throw exceptions.
*/

void throw_value_error( value v )
{
    throw value_error( v );
}

void throw_type_error( value v, const char* expected )
{
    throw type_error( v, expected );
}

/*
    Unwind.
*/

void unwind( vmachine* vm, script_error* e, unsigned ip )
{
    cothread_object* cothread = vm->cothreads->back();
    stack_frame& frame = cothread->stack_frames.back();
    frame.ip = ip;

    while ( true )
    {
        cothread_object* cothread = vm->cothreads->back();
        stack_frame& frame = cothread->stack_frames.back();
        if ( ! frame.function )
        {
            return;
        }

        program_object* program = read( frame.function->program );

        unsigned ip = frame.ip - 1;
        std::string_view fname = program_name( vm, program );
        std::string_view sname = script_name( vm, read( program->script ) );
        source_location sloc = program_source_location( vm, program, ip );
        e->append_stack_trace( "%.*s:%u:%u: %.*s", (int)sname.size(), sname.data(), sloc.line, sloc.column, (int)fname.size(), fname.data() );

        cothread->stack_frames.pop_back();
        if ( cothread->stack_frames.empty() )
        {
            if ( vm->cothreads->size() > 1 )
                vm->cothreads->pop_back();
            else
                break;
        }
    }
}

}

