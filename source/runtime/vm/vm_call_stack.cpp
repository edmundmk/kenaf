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

value* vm_resize_stack( vm_context* vm, unsigned xp )
{
    cothread_object* cothread = vm->cothreads.back();
    const vm_stack_frame& stack_frame = cothread->stack_frames.back();

    // xp is relative to current frame pointer.
    xp += stack_frame.fp;

    // Increase stack size.
    if ( xp > cothread->stack.size() )
    {
        size_t size = xp + 31u & ~(size_t)31u;
        cothread->stack.resize( size );
    }

    // Return possibly reallocated stack.
    return cothread->stack.data() + stack_frame.fp;
}

value* vm_entire_stack( vm_context* vm )
{
    cothread_object* cothread = vm->cothreads.back();
    return cothread->stack.data();
}


}

