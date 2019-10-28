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

#include "../datatypes/hash_table.h"
#include "../datatypes/segment_list.h"
#include "../objects/lookup_object.h"
#include "../objects/string_object.h"

namespace kf
{

struct vm_context
{
    vm_context();
    ~vm_context();

    // List of root objects.
    hash_table< object*, size_t > roots;

    // Lookup object tables.
    hash_table< string_hashkey, string_object* > keys;
    hash_table< lookup_object*, layout_object* > instance_layouts;
    hash_table< layout_hashkey, layout_object* > splitkey_layouts;
    uint32_t next_cookie;

};

}

#endif

