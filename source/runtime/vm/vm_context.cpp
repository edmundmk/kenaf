//
//  vm_context.cpp
//
//  Created by Edmund Kapusniak on 29/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "vm_context.h"
#include "../objects/cothread_object.h"

namespace kf
{

value* vm_active_stack( vm_context* vm, vm_stack_frame* out_frame )
{
    cothread_object* cothread = vm->cothreads.back();
    *out_frame = cothread->stack_frames.back();
    return cothread->stack.data() + cothread->stack_frames.back().fp;
}

value* vm_ensure_stack( vm_context* vm, unsigned xp )
{
    cothread_object* cothread = vm->cothreads.back();
    size_t size = std::max< size_t >( xp, cothread->stack.size() ) + 31 & ~(size_t)31;
    cothread->stack.resize( size );
    return cothread->stack.data() + cothread->stack_frames.back().fp;
}

value* vm_entire_stack( vm_context* vm, vm_stack_frame* out_frame )
{
    cothread_object* cothread = vm->cothreads.back();
    *out_frame = cothread->stack_frames.back();
    return cothread->stack.data();
}

value* vm_prologue( vm_context* vm, unsigned rp, unsigned xp, vm_stack_frame* out_frame )
{
}

value* vm_epilogue( vm_context* vm, unsigned rp, unsigned xp, vm_stack_frame* out_frame )
{
}

value* vm_generate( vm_context* vm, cothread_object* cothread, unsigned rp, unsigned xp, vm_stack_frame* out_frame )
{
}

value* vm_genyield( vm_context* vm, unsigned rp, unsigned ap, unsigned xp, vm_stack_frame* out_frame )
{
}

}

