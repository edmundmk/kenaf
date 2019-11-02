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

vm_stack_state vm_active_state( vm_context* vm )
{
    cothread_object* cothread = vm->cothreads->back();
    const vm_stack_frame& stack_frame = cothread->stack_frames.back();
    return
    {
        stack_frame.function,
        cothread->stack.data() + stack_frame.fp,
        stack_frame.ip,
        cothread->xp - stack_frame.fp
    };
}

vm_stack_frame* vm_active_frame( vm_context* vm )
{
    return &vm->cothreads->back()->stack_frames.back();
}

static value* vm_resize_stack( cothread_object* cothread, const vm_stack_frame* stack_frame, unsigned xp )
{
    // xp is relative to current frame pointer.
    xp += stack_frame->fp;

    // Increase stack size.
    if ( xp > cothread->stack.size() )
    {
        size_t size = ( xp + 31u ) & ~(size_t)31u;
        cothread->stack.resize( size );
    }

    // Return possibly reallocated stack.
    return cothread->stack.data() + stack_frame->fp;
}

value* vm_resize_stack( vm_context* vm, unsigned xp )
{
    cothread_object* cothread = vm->cothreads->back();
    return vm_resize_stack( vm->cothreads->back(), &cothread->stack_frames.back(), xp );
}

value* vm_entire_stack( vm_context* vm )
{
    cothread_object* cothread = vm->cothreads->back();
    return cothread->stack.data();
}

vm_stack_state vm_call( vm_context* vm, function_object* function, unsigned rp, unsigned xp )
{
    assert( rp < xp );
    program_object* program = read( function->program );
    bool is_varargs = ( program->code_flags & CODE_VARARGS ) != 0;

    cothread_object* cothread = vm->cothreads->back();
    value* r = cothread->stack.data() + cothread->stack_frames.back().fp;

    cothread->stack_frames.push_back( { function, rp, rp, 0, VM_ACTIVE, 0, 0, 0 } );
    vm_stack_frame* stack_frame = &cothread->stack_frames.back();

    unsigned argument_count = xp - ( rp + 1 );
    if ( argument_count < program->param_count || ( argument_count > program->param_count && ! is_varargs ) )
    {
        throw std::exception();
    }

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
        unsigned reverse_ap = stack_frame->bp + ( program->param_count + 1 );
        std::reverse( r + stack_frame->bp, r + reverse_ap );
        std::reverse( r + reverse_ap, r + xp );
        std::reverse( r + stack_frame->bp, r + xp );
        stack_frame->fp = stack_frame->bp + argument_count - ( program->param_count + 1 );
    }

    r = vm_resize_stack( cothread, stack_frame, program->stack_size );
    return { stack_frame->function, r, stack_frame->ip, cothread->xp - stack_frame->fp };
}

vm_stack_state vm_call_native( vm_context* vm, native_function_object* function, unsigned rp, unsigned xp )
{
    assert( rp < xp );
    cothread_object* cothread = vm->cothreads->back();
    vm_stack_frame* stack_frame = &cothread->stack_frames.back();
    stack stack = { cothread, stack_frame->fp + rp };

    unsigned argument_count = xp - ( rp + 1 );
    bool is_varargs = ( function->code_flags & CODE_VARARGS ) != 0;
    if ( argument_count < function->param_count || ( argument_count > function->param_count && ! is_varargs ) )
    {
        throw std::exception();
    }

    value* argv = cothread->stack.data() + stack.fp + 1;
    size_t result_count = function->native( function->cookie, &stack, argv, argument_count );

    if ( stack_frame->call == VM_CONSTRUCT )
    {
        result_count = 0;
    }

    stack_frame = &cothread->stack_frames.back();
    unsigned xr = stack_frame->xr;
    unsigned xb = stack_frame->xb != OP_STACK_MARK ? stack_frame->xb : xr + result_count;
    value* r = vm_resize_stack( cothread, stack_frame, xb );

    if ( stack_frame->call == VM_CONSTRUCT && result_count == 0 )
    {
        xr += 1;
    }

    while ( xr < xb )
    {
        r[ xr++ ] = boxed_null;
    }

    if ( stack_frame->rr != stack_frame->xr )
    {
        r[ stack_frame->rr ] = r[ stack_frame->xr ];
    }

    return { stack_frame->function, r, stack_frame->ip, cothread->xp - stack_frame->fp };
}

static vm_stack_state vm_yield_return( vm_context* vm, cothread_object* cothread, const vm_stack_frame* stack_frame, const value* yield_r, unsigned rp, unsigned xp )
{
    assert( rp <= xp );

    // Copy results.
    size_t result_count = rp - xp;
    unsigned xr = stack_frame->xr;
    unsigned xb = stack_frame->xb != OP_STACK_MARK ? stack_frame->xb : xr + result_count;
    value* r = vm_resize_stack( cothread, stack_frame, xb );

    if ( stack_frame->call == VM_CONSTRUCT && result_count == 0 )
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

vm_stack_state vm_return( vm_context* vm, unsigned rp, unsigned xp )
{
    assert( rp <= xp );

    // Get current stack.
    cothread_object* cothread = vm->cothreads->back();
    vm_stack_frame return_frame = cothread->stack_frames.back();

    if ( cothread->stack_frames.size() )
    {
        // Normal return.
        vm_stack_frame* stack_frame = &cothread->stack_frames.back();
        size_t result_count = rp - xp;
        unsigned xr = stack_frame->xr;
        unsigned xb = stack_frame->xb != OP_STACK_MARK ? stack_frame->xb : xr + result_count;

        // Get everything relative to entire stack.
        value* stack = cothread->stack.data();
        value* return_r = stack + return_frame.fp;
        value* r = stack + stack_frame->fp;

        assert( r <= return_r );
        assert( r + xr <= return_r + rp );
        assert( stack_frame->fp + xb <= cothread->stack.size() );

        if ( stack_frame->call == VM_CONSTRUCT && result_count == 0 )
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
    else
    {
        // Complete cothread.
        cothread_object* return_cothread = cothread;
        vm->cothreads->pop_back();

        assert( ! vm->cothreads->empty() );
        cothread = vm->cothreads->back();
        vm_stack_frame* stack_frame = &cothread->stack_frames.back();

        if ( stack_frame->call != VM_FOR_EACH )
        {
            // Return across cothreads->
            value* yield_r = return_cothread->stack.data() + return_frame.fp;
            return vm_yield_return( vm, cothread, stack_frame, yield_r, rp, xp );
        }
        else
        {
            // No results, end iteration by jumping.
            value* r = cothread->stack.data() + stack_frame->fp;
            return { stack_frame->function, r, stack_frame->ip - 1, cothread->xp - stack_frame->fp };
        }
    }
}

vm_stack_state vm_generate( vm_context* vm, function_object* function, unsigned rp, unsigned xp )
{
    assert( rp < xp );
    program_object* program = read( function->program );
    bool is_varargs = ( program->code_flags & CODE_VARARGS ) != 0;

    // Create new cothread.
    cothread_object* generator_cothread = cothread_new( vm );

    // Get current stack.
    cothread_object* caller_cothread = vm->cothreads->back();
    value* caller_r = caller_cothread->stack.data() + caller_cothread->stack_frames.back().fp;

    // Insert stack frame in new cothread.
    generator_cothread->stack_frames.push_back( { function, 0, 0, 0, VM_YIELD, 0, 0, 0 } );
    vm_stack_frame* stack_frame = &generator_cothread->stack_frames.back();

    unsigned argument_count = xp - ( rp + 1 );
    if ( argument_count < program->param_count || ( argument_count > program->param_count && ! is_varargs ) )
    {
        throw std::exception();
    }

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
    value* generator_r = vm_resize_stack( generator_cothread, stack_frame, stack_size );
    unsigned actual_count = program->param_count + 1;
    unsigned vararg_count = xp - rp - actual_count;
    memcpy( generator_r, caller_r + rp + actual_count, vararg_count * sizeof( value ) );
    memcpy( generator_r + vararg_count, caller_r + rp, actual_count * sizeof( value ) );
    stack_frame->fp = vararg_count;

    // Continue with current stack frame.
    vm_stack_frame* caller_frame = &caller_cothread->stack_frames.back();
    return { caller_frame->function, caller_r, caller_frame->ip, caller_cothread->xp - caller_frame->fp };
}

vm_stack_state vm_resume( vm_context* vm, cothread_object* cothread, unsigned rp, unsigned xp )
{
    assert( rp <= xp );

    // Get current stack.
    cothread_object* caller_cothread = vm->cothreads->back();
    if ( caller_cothread->stack_frames.empty() )
    {
        // Cothread has completed.
        throw std::exception();
    }

    value* caller_r = caller_cothread->stack.data() + caller_cothread->stack_frames.back().fp;

    // Get stack frame we are resuming into.
    vm_stack_frame* stack_frame = &cothread->stack_frames.back();
    assert( stack_frame->call == VM_YIELD );
    assert( stack_frame->rr == stack_frame->xr );

    // Work out place in stack to copy to.
    unsigned xr = stack_frame->xr;
    unsigned xb = stack_frame->xb != OP_STACK_MARK ? stack_frame->xb : xr + ( rp - xp );
    value* r = vm_resize_stack( cothread, stack_frame, xb );

    // Copy parameters into cothread.
    while ( xr < xb )
    {
        r[ xr++ ] = rp < xp ? caller_r[ rp++ ] : boxed_null;
    }

    // Continue with new cothread.
    vm->cothreads->push_back( cothread );
    return { stack_frame->function, r, stack_frame->ip, cothread->xp - stack_frame->fp };
}

vm_stack_state vm_yield( vm_context* vm, unsigned rp, unsigned xp )
{
    assert( rp >= xp );

    // Get current stack.
    cothread_object* yield_cothread = vm->cothreads->back();
    value* yield_r = yield_cothread->stack.data() + yield_cothread->stack_frames.back().fp;
    vm->cothreads->pop_back();

    // Get cothread we are yielding into.
    assert( vm->cothreads->size() );
    cothread_object* cothread = vm->cothreads->back();
    vm_stack_frame* stack_frame = &cothread->stack_frames.back();

    // Return across cothreads->
    return vm_yield_return( vm, cothread, stack_frame, yield_r, rp, xp );
}

}

