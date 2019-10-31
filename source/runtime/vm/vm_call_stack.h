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

#include <stdint.h>

namespace kf
{

struct vm_context;
struct function_object;
struct cothread_object;
struct value;

const unsigned VM_STACK_MARK = ~(unsigned)0;

struct vm_stack_frame
{
    function_object* function;

    unsigned bp;        // base pointer
    unsigned fp;        // frame pointer
    unsigned ip;        // instruction pointer

    uint8_t xr;         // lower index of call/yield results
    uint8_t xb;         // upper index of call/yield results
    uint8_t rr;         // callr result register
    uint8_t construct;  // called into a constructor
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
value* vm_entire_stack( vm_context* vm );

vm_stack_state vm_call_function( vm_context* vm, function_object* function, unsigned rp, unsigned xp );
vm_stack_state vm_create_cothread( vm_context* vm, function_object* function, unsigned rp, unsigned xp );
vm_stack_state vm_resume_cothread( vm_context* vm, cothread_object* cothread, unsigned rp, unsigned xp );
vm_stack_state vm_yield_cothread( vm_context* vm, unsigned rp, unsigned xp );
vm_stack_state vm_return_function( vm_context* vm, unsigned rp, unsigned xp );

}

#endif
