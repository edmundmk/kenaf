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
    cothread_object* cothread = vm->cothreads.back();
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
    return &vm->cothreads.back()->stack_frames.back();
}

static value* vm_resize_stack( cothread_object* cothread, unsigned xp )
{
    const vm_stack_frame& stack_frame = cothread->stack_frames.back();

    // xp is relative to current frame pointer.
    xp += stack_frame.fp;

    // Increase stack size.
    if ( xp > cothread->stack.size() )
    {
        size_t size = ( xp + 31u ) & ~(size_t)31u;
        cothread->stack.resize( size );
    }

    // Return possibly reallocated stack.
    return cothread->stack.data() + stack_frame.fp;
}

value* vm_resize_stack( vm_context* vm, unsigned xp )
{
    return vm_resize_stack( vm->cothreads.back(), xp );
}

value* vm_entire_stack( vm_context* vm )
{
    cothread_object* cothread = vm->cothreads.back();
    return cothread->stack.data();
}

vm_stack_state vm_call( vm_context* vm, function_object* function, unsigned rp, unsigned xp )
{
    assert( xp > rp );
    program_object* program = read( function->program );
    bool is_varargs = ( program->code_flags & CODE_VARARGS ) != 0;

    cothread_object* cothread = vm->cothreads.back();
    value* r = cothread->stack.data();
    unsigned fp = cothread->stack_frames.back().fp;
    rp += fp;
    xp += fp;

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

    r = vm_resize_stack( cothread, stack_frame->fp + program->stack_size );
    return { stack_frame->function, r, stack_frame->ip, 0 };
}

vm_stack_state vm_return( vm_context* vm, unsigned rp, unsigned xp )
{
    return {};
}

vm_stack_state vm_generate( vm_context* vm, function_object* function, unsigned rp, unsigned xp )
{
    assert( xp > rp );
    program_object* program = read( function->program );
    bool is_varargs = ( program->code_flags & CODE_VARARGS ) != 0;

    // Create new cothread.
    cothread_object* generator_cothread = cothread_new( vm );

    // Get current stack.
    cothread_object* caller_cothread = vm->cothreads.back();
    value* caller_r = caller_cothread->stack.data();
    unsigned caller_fp = caller_cothread->stack_frames.back().fp;
    rp += caller_fp;
    xp += caller_fp;

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
    value* generator_r = vm_resize_stack( generator_cothread, std::max< unsigned >( program->stack_size, 1 + argument_count ) );
    unsigned actual_count = program->param_count + 1;
    unsigned vararg_count = xp - rp - actual_count;
    memcpy( generator_r, caller_r + rp + actual_count, vararg_count );
    memcpy( generator_r + vararg_count, caller_r + rp, actual_count );
    stack_frame->fp = vararg_count;

    // Continue with current stack frame.
    vm_stack_frame* caller_frame = &caller_cothread->stack_frames.back();
    return { caller_frame->function, caller_r + caller_frame->fp, caller_frame->ip, caller_cothread->xp - caller_frame->fp };
}

vm_stack_state vm_resume( vm_context* vm, cothread_object* cothread, unsigned rp, unsigned xp )
{
    return {};
}

vm_stack_state vm_yield( vm_context* vm, unsigned rp, unsigned xp )
{
    return {};
}


}

