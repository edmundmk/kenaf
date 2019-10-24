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

#include "datatypes/hash_table.h"
#include "datatypes/segment_list.h"

namespace kf
{

struct gc_header;

struct vm_context
{
    // Write barrier mark state.
    uint8_t mark_color;
    segment_list< gc_header* > mark_list;

    // Runtime tables.




};

}

#endif

