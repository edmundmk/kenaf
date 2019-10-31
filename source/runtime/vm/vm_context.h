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
    Context structure storing runtime state.
*/

#include <vector>
#include "../datatypes/hash_table.h"
#include "../datatypes/segment_list.h"
#include "../objects/lookup_object.h"
#include "../objects/string_object.h"

namespace kf
{

struct function_object;
struct cothread_object;

/*
    Global VM state.
*/

struct vm_context
{
    vm_context();
    ~vm_context();

    // Context state.
    std::vector< cothread_object* > cothreads;
    lookup_object* global_object;
    lookup_object* prototypes[ TYPE_COUNT ];

    // Lookup object tables.
    hash_table< string_hashkey, string_object* > keys;
    hash_table< lookup_object*, layout_object* > instance_layouts;
    hash_table< layout_hashkey, layout_object* > splitkey_layouts;
    uint32_t next_cookie;

    // List of root objects.
    hash_table< object*, size_t > roots;
};

/*
    Dealing with call stacks.  Stack frames have this general layout:

        bp  ->  vararg 0
                vararg 1
        fp  ->  function
                argument 0
                argument 1

    rp:xp is relative to fp, and tells us where on our stack the results of a
    call need to be placed.  A call frame without a function object means to
    return to native code.
*/

const unsigned VM_STACK_MARK = ~(unsigned)0;

struct vm_stack_frame
{
    function_object* function;
    unsigned ip;
    unsigned bp;
    unsigned fp;
    unsigned rp : 8;
    unsigned xp : 24;
};

value* vm_active_stack( vm_context* vm, vm_stack_frame* out_frame );
value* vm_ensure_stack( vm_context* vm, unsigned xp );
value* vm_entire_stack( vm_context* vm, vm_stack_frame* out_frame );

value* vm_prologue( vm_context* vm, unsigned rp, unsigned ap, unsigned xp, vm_stack_frame* out_frame );
value* vm_epilogue( vm_context* vm, unsigned rp, unsigned xp, vm_stack_frame* out_frame );
value* vm_generate( vm_context* vm, cothread_object* cothread, unsigned rp, unsigned xp, vm_stack_frame* out_frame );
value* vm_genyield( vm_context* vm, unsigned rp, unsigned ap, unsigned xp, vm_stack_frame* out_frame );

}

#endif

