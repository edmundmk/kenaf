//
//  vm_call_stack.h
//
//  Created by Edmund Kapusniak on 31/10/2019.
//  Copyright © 2019 Edmund Kapusniak. All rights reserved.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_VM_CALL_STACK_H
#define KF_VM_CALL_STACK_H

/*
    Dealing with call stacks.  Stack frames have this general layout:

        bp  ->  vararg 0
                vararg 1
        fp  ->  function
                argument 0
                argument 1

    xr:xb is relative to fp, and tells us where on our stack the results of a
    call or yield need to be placed.  A call frame without a function object
    means to return to native code.
*/

#include <stddef.h>
#include <stdint.h>

namespace kf
{

struct vm_context;
struct function_object;
struct native_function_object;
struct cothread_object;
struct value;

enum vm_resume : uint8_t
{
    RESUME_CALL,        // return doesn't need to do anything special
    RESUME_YIELD,       // return
    RESUME_CONSTRUCT,   // if returning zero results, preserve self
    RESUME_FOR_EACH,    // if generator is done, return to jump
};

struct vm_stack_frame
{
    function_object* function;

    unsigned bp;        // base pointer
    unsigned fp;        // frame pointer
    unsigned ip;        // instruction pointer

    vm_resume resume;   // resume kind.
    uint8_t xr;         // lower index of call/yield results
    uint8_t xb;         // upper index of call/yield results
    uint8_t rr;         // callr result register
};

struct vm_native_frame
{
    cothread_object* cothread;
    unsigned fp;
};

struct vm_stack_state
{
    function_object* function;
    value* r;
    unsigned ip;
    unsigned xp;
};

vm_stack_state vm_active_state( vm_context* vm );
vm_stack_frame* vm_active_frame( vm_context* vm );
value* vm_resize_stack( vm_context* vm, unsigned xp );
value* vm_resize_stack( cothread_object* cothread, unsigned fp, unsigned xp );
value* vm_entire_stack( vm_context* vm );

vm_stack_state vm_call( vm_context* vm, function_object* function, unsigned rp, unsigned xp );
vm_stack_state vm_call_native( vm_context* vm, native_function_object* function, unsigned rp, unsigned xp );
vm_stack_state vm_call_generator( vm_context* vm, function_object* function, unsigned rp, unsigned xp );
vm_stack_state vm_call_cothread( vm_context* vm, cothread_object* cothread, unsigned rp, unsigned xp );

vm_stack_state vm_return( vm_context* vm, unsigned rp, unsigned xp );
vm_stack_state vm_yield( vm_context* vm, unsigned rp, unsigned xp );

}

#endif
