//
//  call_stack.h
//
//  Created by Edmund Kapusniak on 24/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_CALL_STACK_H
#define KF_CALL_STACK_H

/*
    Functions dealing with the virtual machine's call/cothread stack.
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

struct xstate
{
    function_object* function;
    value* r;
    unsigned ip;
    unsigned xp;
};

/*
    Call stack.
*/

enum resume_kind : uint8_t
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

    resume_kind resume; // resume kind.
    uint8_t xr;         // lower index of call/yield results
    uint8_t xb;         // upper index of call/yield results
    uint8_t rr;         // callr result register
};

/*
    Functions for manipulating the register stack.
*/

stack_frame* active_frame( vmachine* vm );
value* resize_stack( vmachine* vm, unsigned xp );
value* resize_stack( cothread_object* cothread, unsigned fp, unsigned xp );
value* entire_stack( vmachine* vm );

/*
    Functions that perform calls and returns.
*/

xstate call_function( vmachine* vm, function_object* function, unsigned rp, unsigned xp );
xstate call_native( vmachine* vm, native_function_object* function, unsigned rp, unsigned xp );
xstate call_generator( vmachine* vm, function_object* function, unsigned rp, unsigned xp );
xstate call_cothread( vmachine* vm, cothread_object* cothread, unsigned rp, unsigned xp );

xstate call_return( vmachine* vm, unsigned rp, unsigned xp );
xstate call_yield( vmachine* vm, unsigned rp, unsigned xp );

/*
    Throwing of exceptions from execute loop.
*/

[[noreturn]] void throw_value_error( value v );
[[noreturn]] void throw_type_error( value v, const char* expected );

/*
    Handle unwind.
*/

void unwind( vmachine* vm, script_error* e, unsigned ip );

}

#endif

