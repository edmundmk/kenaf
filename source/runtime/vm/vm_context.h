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
#include "../datatypes/hash_table.h"
#include "../datatypes/segment_list.h"
#include "../objects/lookup_object.h"
#include "../objects/string_object.h"
#include "../objects/function_object.h"

namespace kf
{

struct cothread_object;

/*
    Global VM state.
*/

typedef std::vector< cothread_object* > vm_cothread_stack;

struct vm_context
{
    vm_context();
    ~vm_context();

    // Context state.
    vm_cothread_stack* cothreads;
    lookup_object* global_object;

    // Object model support.
    lookup_object* prototypes[ TYPE_COUNT ];
    string_object* self_key;
    selector self_sel;

    // Lookup object tables.
    hash_table< string_hashkey, string_object* > keys;
    hash_table< lookup_object*, layout_object* > instance_layouts;
    hash_table< layout_hashkey, layout_object* > splitkey_layouts;
    uint32_t next_cookie;

    // List of root objects.
    hash_table< object*, size_t > roots;
};

void vm_setup_object_model( vm_context* vm );

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

/*
    Functions for manipulating the register stack.
*/

vm_stack_frame* vm_active_frame( vm_context* vm );
value* vm_resize_stack( vm_context* vm, unsigned xp );
value* vm_resize_stack( cothread_object* cothread, unsigned fp, unsigned xp );
value* vm_entire_stack( vm_context* vm );

/*
    Functions that perform calls and returns.
*/

vm_exstate vm_call( vm_context* vm, function_object* function, unsigned rp, unsigned xp );
vm_exstate vm_call_native( vm_context* vm, native_function_object* function, unsigned rp, unsigned xp );
vm_exstate vm_call_generator( vm_context* vm, function_object* function, unsigned rp, unsigned xp );
vm_exstate vm_call_cothread( vm_context* vm, cothread_object* cothread, unsigned rp, unsigned xp );

vm_exstate vm_return( vm_context* vm, unsigned rp, unsigned xp );
vm_exstate vm_yield( vm_context* vm, unsigned rp, unsigned xp );

/*
    Common object model operations.
*/

lookup_object* vm_keyerof( vm_context* vm, value object );
lookup_object* vm_superof( vm_context* vm, value object );

}

#endif

