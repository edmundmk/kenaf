//
//  vm_context.h
//
//  Created by Edmund Kapusniak on 24/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_VM_CONTEXT_H
#define KF_VM_CONTEXT_H

/*
    Context structure storing runtime state, and functions to manipulate the
    context's state that don't belong in the main interpreter loop.
*/

#include <vector>
#include "../vmachine.h"
#include "../objects/lookup_object.h"
#include "../objects/string_object.h"
#include "../objects/function_object.h"

namespace kf
{

/*
    Execute state, required to execute bytecode.
*/

struct vm_exstate
{
    function_object* function;
    value* r;
    unsigned ip;
    unsigned xp;
};

/*
    Call stack.
*/

enum vm_resume : uint8_t
{
    RESUME_CALL,        // return doesn't need to do anything special
    RESUME_YIELD,       // return
    RESUME_CONSTRUCT,   // if returning zero results, preserve self
    RESUME_FOR_EACH,    // if generator is done, return to jump
};

struct stack_frame
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

/*
    Functions for manipulating the register stack.
*/

stack_frame* vm_active_frame( vmachine* vm );
value* vm_resize_stack( vmachine* vm, unsigned xp );
value* vm_resize_stack( cothread_object* cothread, unsigned fp, unsigned xp );
value* vm_entire_stack( vmachine* vm );

/*
    Functions that perform calls and returns.
*/

vm_exstate vm_call( vmachine* vm, function_object* function, unsigned rp, unsigned xp );
vm_exstate vm_call_native( vmachine* vm, native_function_object* function, unsigned rp, unsigned xp );
vm_exstate vm_call_generator( vmachine* vm, function_object* function, unsigned rp, unsigned xp );
vm_exstate vm_call_cothread( vmachine* vm, cothread_object* cothread, unsigned rp, unsigned xp );

vm_exstate vm_return( vmachine* vm, unsigned rp, unsigned xp );
vm_exstate vm_yield( vmachine* vm, unsigned rp, unsigned xp );

/*
    Throwing of exceptions from execute loop.
*/

[[noreturn]] void vm_throw( value v );
[[noreturn]] void vm_type_error( value v, const char* expected );

/*
    Handle unwind.
*/

void vm_unwind( vmachine* vm, script_error* e, unsigned ip );

}

#endif

