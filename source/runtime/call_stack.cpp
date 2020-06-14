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
#include "collector.h"
#include "objects/cothread_object.h"

namespace kf
{

extern void append_stack_trace( const char* format, ... );

static xstate stack_return( vmachine* vm, cothread_object* cothread, const stack_frame* stack_frame, unsigned return_fp, unsigned rp, unsigned xp );
static xstate yield_return( vmachine* vm, cothread_object* cothread, const stack_frame* stack_frame, const value* yield_r, unsigned rp, unsigned xp );

stack_frame* active_frame( vmachine* vm )
{
    return &vm->c->cothread->stack_frames.back();
}

value* resize_stack( vmachine* vm, unsigned xp )
{
    cothread_object* cothread = vm->c->cothread;
    return resize_stack( cothread, cothread->stack_frames.back().fp, xp );
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
    cothread_object* cothread = vm->c->cothread;
    return cothread->stack.data();
}

bool call_value( vmachine* vm, value u, unsigned rp, unsigned xp, bool ycall, xstate* out_state )
{
    if ( ! box_is_object( u ) ) return false;
    type_code type = header( unbox_object( u ) )->type;

    /*
        Object types that you can call:
            Lookup Objects  Get self method and pass a new object to it plus parameters.
            Functions       Construct call frame for function, continue.
            Generators      Create cothread for generator, assign initial parameters.
            Cothreads       Push cothread on stack, resume yielded cothread.
    */

    if ( type == FUNCTION_OBJECT )
    {
        function_object* callee_function = (function_object*)unbox_object( u );

        // Check for generator.
        if ( ! ycall )
        {
            program_object* callee_program = read( callee_function->program );
            if ( ( callee_program->code_flags & CODE_GENERATOR ) != 0 )
            {
                *out_state = call_generator( vm, callee_function, rp, xp );
                return true;
            }
        }

        // Call normal function.
        *out_state = call_function( vm, callee_function, rp, xp );
        return true;
    }

    if ( type == NATIVE_FUNCTION_OBJECT )
    {
        // Call native function.
        native_function_object* callee_function = (native_function_object*)unbox_object( u );
        *out_state = call_native( vm, callee_function, rp, xp );
        return true;
    }

    if ( type == COTHREAD_OBJECT )
    {
        // Resume yielded cothread.
        cothread_object* callee_cothread = (cothread_object*)unbox_object( u );
        *out_state = call_cothread( vm, callee_cothread, rp, xp );
        return true;
    }

    if ( type == LOOKUP_OBJECT )
    {
        // Call prototype constructor.
        lookup_object* callee_prototype = (lookup_object*)unbox_object( u );
        *out_state = call_prototype( vm, callee_prototype, rp, xp );
        return true;
    }

    return false;
}

xstate call_function( vmachine* vm, function_object* function, unsigned rp, unsigned xp )
{
    /*
        call rp:xp
    */

    assert( rp < xp );

    program_object* program = read( function->program );
    bool is_varargs = ( program->code_flags & CODE_VARARG ) != 0;
    unsigned argument_count = xp - ( rp + 1 );
    if ( argument_count < program->param_count || ( argument_count > program->param_count && ! is_varargs ) )
    {
        raise_error( ERROR_ARGUMENT, "incorrect argument count, expected %u, got %u", program->param_count, argument_count );
    }

    cothread_object* cothread = vm->c->cothread;
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

    bool is_varargs = ( function->code_flags & CODE_VARARG ) != 0;
    unsigned argument_count = xp - ( rp + 1 );
    if ( argument_count < function->param_count || ( argument_count > function->param_count && ! is_varargs ) )
    {
        raise_error( ERROR_ARGUMENT, "incorrect argument count, expected %u, got %u", function->param_count, argument_count );
    }

    cothread_object* cothread = vm->c->cothread;
    size_t frame_count = cothread->stack_frames.size();
    unsigned bp = cothread->stack_frames.back().fp + rp;

    frame native_frame = { cothread, bp };
    value* arguments = cothread->stack.data() + bp + 1;
    size_t result_count;
    try
    {
        result_count = function->native( function->cookie, (frame*)&native_frame, arguments, argument_count );
    }
    catch ( ... )
    {

        append_stack_trace( "[native]: %.*s", (int)function->name_size, function->name_text );
        throw;
    }

    assert( vm->c->cothread == cothread );
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
    bool is_varargs = ( program->code_flags & CODE_VARARG ) != 0;
    unsigned argument_count = xp - ( rp + 1 );
    if ( argument_count < program->param_count || ( argument_count > program->param_count && ! is_varargs ) )
    {
        raise_error( ERROR_ARGUMENT, "incorrect argument count, expected %u, got %u", program->param_count, argument_count );
    }

    // Get current stack.
    cothread_object* caller_cothread = vm->c->cothread;
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

    // Mark cothread.
    cothread = mark_cothread( vm, cothread );

    // Cothread might have completed.
    if ( cothread->stack_frames.empty() )
    {
        raise_error( ERROR_COTHREAD, "cothread is done" );
    }

    // Get current stack.
    cothread_object* caller_cothread = vm->c->cothread;
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
    vm->c->cothread_stack.push_back( caller_cothread );
    vm->c->cothread = cothread;
    return { stack_frame->function, r, stack_frame->ip, cothread->xp - stack_frame->fp };
}


xstate call_prototype( vmachine* vm, lookup_object* prototype, unsigned rp, unsigned xp )
{
    /*
        Call prototype.self, optionally passing a newly-constructed object.
    */

    assert( rp < xp );

    // Lookup prototype.self.
    value c = lookup_getkey( vm, prototype, vm->self_key, &vm->self_sel );
    if ( ! box_is_object( c ) ) raise_type_error( c, "a function" );
    type_code type = header( unbox_object( c ) )->type;

    if ( ( header( unbox_object( c ) )->flags & FLAG_DIRECT ) == 0 )
    {
        // Construct new object.
        lookup_object* self = lookup_new( vm, prototype );

        // Rearrange stack.  self must be kept live.
        value* r = resize_stack( vm, xp + 2 );
        memmove( r + rp + 2, r + rp, sizeof( value ) * ( xp - rp ) );
        r[ rp + 0 ] = box_object( self );
        r[ rp + 1 ] = c;
        r[ rp + 2 ] = box_object( self );
        rp += 1;
        xp += 2;

        // Return needs to know about the self parameter.
        stack_frame* stack_frame = active_frame( vm );
        assert( stack_frame->resume == RESUME_CALL );
        stack_frame->resume = RESUME_CONSTRUCT;
    }
    else
    {
        // Shift stack to add null self parameter.
        value* r = resize_stack( vm, xp + 1 );
        memmove( r + rp + 1, r + rp, sizeof( value ) * ( xp - rp ) );
        r[ rp + 0 ] = c;
        r[ rp + 1 ] = boxed_null;
        xp += 1;
    }

    // Now call actual constructor.
    if ( type == FUNCTION_OBJECT )
    {
        function_object* callee_function = (function_object*)unbox_object( c );
        program_object* callee_program = read( callee_function->program );
        if ( ( callee_program->code_flags & CODE_GENERATOR ) != 0 )
        {
            raise_type_error( c, "a non-generator function" );
        }
        return call_function( vm, callee_function, rp, xp );
    }
    else if ( type == NATIVE_FUNCTION_OBJECT )
    {
        native_function_object* callee_function = (native_function_object*)unbox_object( c );
        return call_native( vm, callee_function, rp, xp );
    }
    else
    {
        raise_type_error( c, "a function" );
    }
}

xstate call_return( vmachine* vm, unsigned rp, unsigned xp )
{
    assert( rp <= xp );

    // Get current stack.
    cothread_object* cothread = vm->c->cothread;
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

        assert( ! vm->c->cothread_stack.empty() );
        vm->c->cothread = mark_cothread( vm, vm->c->cothread_stack.back() );
        vm->c->cothread_stack.pop_back();

        cothread = vm->c->cothread;
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
    cothread_object* yield_cothread = vm->c->cothread;
    value* yield_r = yield_cothread->stack.data() + yield_cothread->stack_frames.back().fp;

    // Get cothread we are yielding into.
    assert( ! vm->c->cothread_stack.empty() );
    vm->c->cothread = mark_cothread( vm, vm->c->cothread_stack.back() );
    vm->c->cothread_stack.pop_back();

    cothread_object* cothread = vm->c->cothread;
    const stack_frame* stack_frame = &cothread->stack_frames.back();

    // Return across cothreads.
    return yield_return( vm, cothread, stack_frame, yield_r, rp, xp );
}

static xstate stack_return( vmachine* vm, cothread_object* cothread, const stack_frame* stack_frame, unsigned return_fp, unsigned rp, unsigned xp )
{
    assert( rp <= xp );
    size_t result_count = xp - rp;
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

    // Move returned values into position.
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

    // Once stack is resized, this is a GC safepoint.
    cothread->xp = stack_frame->fp + xb;
    safepoint( vm );

    // Continue with current stack frame.
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
    Unwind.
*/

void unwind( vmachine* vm, unsigned ip )
{
    cothread_object* cothread = vm->c->cothread;
    stack_frame& frame = cothread->stack_frames.back();
    frame.ip = ip;

    while ( true )
    {
        cothread_object* cothread = vm->c->cothread;
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
        append_stack_trace( "%.*s:%u:%u: %.*s", (int)sname.size(), sname.data(), sloc.line, sloc.column, (int)fname.size(), fname.data() );

        cothread->stack_frames.pop_back();
        if ( cothread->stack_frames.empty() )
        {
            if ( ! vm->c->cothread_stack.empty() )
            {
                vm->c->cothread = mark_cothread( vm, vm->c->cothread_stack.back() );
                vm->c->cothread_stack.pop_back();
            }
            else
            {
                break;
            }
        }
    }
}

}

