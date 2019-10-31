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

enum vm_stack_call : uint8_t
{
    VM_ACTIVE,          // function is active
    VM_CALL,            // function is at a call op
    VM_CONSTRUCT,       // function is at a call to a constructor
    VM_FOR_EACH,        // function is at a call to a generator
    VM_YIELD,           // function has yielded
};

struct vm_stack_frame
{
    function_object* function;

    unsigned bp;        // base pointer
    unsigned fp;        // frame pointer
    unsigned ip;        // instruction pointer

    vm_stack_call call; // call kind.
    uint8_t xr;         // lower index of call/yield results
    uint8_t xb;         // upper index of call/yield results
    uint8_t rr;         // callr result register
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

vm_stack_state vm_call( vm_context* vm, function_object* function, unsigned rp, unsigned xp );
vm_stack_state vm_return( vm_context* vm, unsigned rp, unsigned xp );

vm_stack_state vm_generate( vm_context* vm, function_object* function, unsigned rp, unsigned xp );
vm_stack_state vm_resume( vm_context* vm, cothread_object* cothread, unsigned rp, unsigned xp );
vm_stack_state vm_yield( vm_context* vm, unsigned rp, unsigned xp );

}

#endif
