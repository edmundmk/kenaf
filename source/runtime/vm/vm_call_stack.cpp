//
//  vm_call_stack.cpp
//
//  Created by Edmund Kapusniak on 31/10/2019.
//  Copyright © 2019 Edmund Kapusniak. All rights reserved.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "vm_call_stack.h"
#include "vm_context.h"
#include "../objects/cothread_object.h"

namespace kf
{

static vm_exstate vm_stack_return( vm_context* vm, cothread_object* cothread, const vm_stack_frame* stack_frame, unsigned return_fp, unsigned rp, unsigned xp );
static vm_exstate vm_yield_return( vm_context* vm, cothread_object* cothread, const vm_stack_frame* stack_frame, const value* yield_r, unsigned rp, unsigned xp );

vm_stack_frame* vm_active_frame( vm_context* vm )
{
    return &vm->cothreads->back()->stack_frames.back();
}

value* vm_resize_stack( vm_context* vm, unsigned xp )
{
    cothread_object* cothread = vm->cothreads->back();
    return vm_resize_stack( vm->cothreads->back(), cothread->stack_frames.back().fp, xp );
}

value* vm_resize_stack( cothread_object* cothread, unsigned fp, unsigned xp )
{
    // xp is relative to current frame pointer.
    xp += fp;

    // Increase stack size.
    if ( xp > cothread->stack.size() )
    {
        size_t size = ( xp + 31u ) & ~(size_t)31u;
        cothread->stack.resize( size );
    }

    // Return possibly reallocated stack.
    return cothread->stack.data() + fp;
}

value* vm_entire_stack( vm_context* vm )
{
    cothread_object* cothread = vm->cothreads->back();
    return cothread->stack.data();
}

vm_exstate vm_call( vm_context* vm, function_object* function, unsigned rp, unsigned xp )
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
        throw std::exception();
    }

    cothread_object* cothread = vm->cothreads->back();
    unsigned bp = cothread->stack_frames.back().fp + rp;
    cothread->stack_frames.push_back( { function, bp, bp, 0, RESUME_CALL, 0, 0, 0 } );
    vm_stack_frame* stack_frame = &cothread->stack_frames.back();

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

    value* r = vm_resize_stack( cothread, stack_frame->fp, program->stack_size );
    return { stack_frame->function, r, stack_frame->ip, cothread->xp - stack_frame->fp };
}

vm_exstate vm_call_native( vm_context* vm, native_function_object* function, unsigned rp, unsigned xp )
{
    /*
        call native rp:xp -> rp:count
    */

    assert( rp < xp );

    bool is_varargs = ( function->code_flags & CODE_VARARGS ) != 0;
    unsigned argument_count = xp - ( rp + 1 );
    if ( argument_count < function->param_count || ( argument_count > function->param_count && ! is_varargs ) )
    {
        throw std::exception();
    }

    cothread_object* cothread = vm->cothreads->back();
    size_t frame_count = cothread->stack_frames.size();
    unsigned bp = cothread->stack_frames.back().fp + rp;

    vm_native_frame native_frame = { cothread, bp };
    value* arguments = cothread->stack.data() + bp + 1;
    size_t result_count = function->native( function->cookie, (frame*)&native_frame, arguments, argument_count );

    assert( vm->cothreads->back() == cothread );
    assert( cothread->stack_frames.size() == frame_count );

    const vm_stack_frame* stack_frame = &cothread->stack_frames.back();
    return vm_stack_return( vm, cothread, stack_frame, bp, 0, result_count );
}

vm_exstate vm_call_generator( vm_context* vm, function_object* function, unsigned rp, unsigned xp )
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
        throw std::exception();
    }

    // Get current stack.
    cothread_object* caller_cothread = vm->cothreads->back();
    const vm_stack_frame* caller_frame = &caller_cothread->stack_frames.back();
    unsigned caller_bp = caller_frame->fp + rp;
    value* caller_r = caller_cothread->stack.data() + caller_bp;

    // Create new cothread.
    cothread_object* generator_cothread = cothread_new( vm );
    generator_cothread->stack_frames.push_back( { function, 0, 0, 0, RESUME_CALL, 0, 0, 0 } );
    vm_stack_frame* generator_frame = &generator_cothread->stack_frames.back();

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
    value* generator_r = vm_resize_stack( generator_cothread, 0, stack_size );
    unsigned actual_count = 1 + program->param_count;
    unsigned vararg_count = xp - rp - actual_count;
    memcpy( generator_r, caller_r + actual_count, vararg_count * sizeof( value ) );
    memcpy( generator_r + vararg_count, caller_r, actual_count * sizeof( value ) );
    generator_frame->fp = vararg_count;

    // Return with the generator as the result.
    caller_r[ 0 ] = box_object( generator_cothread );
    return vm_stack_return( vm, caller_cothread, caller_frame, caller_bp, 0, 1 );
}

vm_exstate vm_call_cothread( vm_context* vm, cothread_object* cothread, unsigned rp, unsigned xp )
{
    /*
        call cothread rp:xp, to new cothread
    */

    assert( rp < xp );
    rp += 1;

    // Cothread might have completed.
    if ( cothread->stack_frames.empty() )
    {
        throw std::exception();
    }

    // Get current stack.
    cothread_object* caller_cothread = vm->cothreads->back();
    value* caller_r = caller_cothread->stack.data() + caller_cothread->stack_frames.back().fp;

    // Get stack frame we are resuming into.
    const vm_stack_frame* stack_frame = &cothread->stack_frames.back();
    assert( stack_frame->resume == RESUME_YIELD );
    assert( stack_frame->rr == stack_frame->xr );

    // Work out place in stack to copy to.
    unsigned xr = stack_frame->xr;
    unsigned xb = stack_frame->xb != OP_STACK_MARK ? stack_frame->xb : xr + ( xp - rp );
    value* r = vm_resize_stack( cothread, stack_frame->fp, xb );

    // Copy parameters into cothread.
    while ( xr < xb )
    {
        r[ xr++ ] = rp < xp ? caller_r[ rp++ ] : boxed_null;
    }

    // Continue with new cothread.
    vm->cothreads->push_back( cothread );
    return { stack_frame->function, r, stack_frame->ip, cothread->xp - stack_frame->fp };
}

vm_exstate vm_return( vm_context* vm, unsigned rp, unsigned xp )
{
    assert( rp <= xp );

    // Get current stack.
    cothread_object* cothread = vm->cothreads->back();
    vm_stack_frame return_frame = cothread->stack_frames.back();
    cothread->stack_frames.pop_back();

    if ( cothread->stack_frames.size() )
    {
        // Normal return.
        const vm_stack_frame* stack_frame = &cothread->stack_frames.back();
        return vm_stack_return( vm, cothread, stack_frame, return_frame.fp, rp, xp );
    }
    else
    {
        // Complete cothread.
        cothread_object* yield_cothread = cothread;
        vm->cothreads->pop_back();

        assert( ! vm->cothreads->empty() );
        cothread = vm->cothreads->back();
        const vm_stack_frame* stack_frame = &cothread->stack_frames.back();

        if ( stack_frame->resume != RESUME_FOR_EACH )
        {
            // Return across cothreads.
            const value* yield_r = yield_cothread->stack.data() + return_frame.fp;
            return vm_yield_return( vm, cothread, stack_frame, yield_r, rp, xp );
        }
        else
        {
            // No results, end iteration by jumping.
            value* r = vm_resize_stack( cothread, stack_frame->fp, rp );
            return { stack_frame->function, r, stack_frame->ip - 1, cothread->xp - stack_frame->fp };
        }
    }
}

vm_exstate vm_yield( vm_context* vm, unsigned rp, unsigned xp )
{
    assert( rp <= xp );

    // Suspend cothread.
    cothread_object* yield_cothread = vm->cothreads->back();
    value* yield_r = yield_cothread->stack.data() + yield_cothread->stack_frames.back().fp;
    vm->cothreads->pop_back();

    // Get cothread we are yielding into.
    assert( ! vm->cothreads->empty() );
    cothread_object* cothread = vm->cothreads->back();
    const vm_stack_frame* stack_frame = &cothread->stack_frames.back();

    // Return across cothreads.
    return vm_yield_return( vm, cothread, stack_frame, yield_r, rp, xp );
}

static vm_exstate vm_stack_return( vm_context* vm, cothread_object* cothread, const vm_stack_frame* stack_frame, unsigned return_fp, unsigned rp, unsigned xp )
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

static vm_exstate vm_yield_return( vm_context* vm, cothread_object* cothread, const vm_stack_frame* stack_frame, const value* yield_r, unsigned rp, unsigned xp )
{
    assert( rp <= xp );

    // Copy results.
    size_t result_count = rp - xp;
    unsigned xr = stack_frame->xr;
    unsigned xb = stack_frame->xb != OP_STACK_MARK ? stack_frame->xb : xr + result_count;
    value* r = vm_resize_stack( cothread, stack_frame->fp, xb );

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

}

