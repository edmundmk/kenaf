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
#include "vmachine.h"
#include "objects/lookup_object.h"
#include "objects/string_object.h"
#include "objects/function_object.h"

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

bool call_value( vmachine* vm, value u, unsigned rp, unsigned xp, bool ycall, xstate* out_state );
xstate call_function( vmachine* vm, function_object* function, unsigned rp, unsigned xp );
xstate call_native( vmachine* vm, native_function_object* function, unsigned rp, unsigned xp );
xstate call_generator( vmachine* vm, function_object* function, unsigned rp, unsigned xp );
xstate call_cothread( vmachine* vm, cothread_object* cothread, unsigned rp, unsigned xp );
xstate call_prototype( vmachine* vm, lookup_object* prototype, unsigned rp, unsigned xp );

xstate call_return( vmachine* vm, unsigned rp, unsigned xp );
xstate call_yield( vmachine* vm, unsigned rp, unsigned xp );

/*
    Handle unwind.
*/

void unwind( vmachine* vm, unsigned ip );

}

#endif

