//
//  object_model.h
//
//  Created by Edmund Kapusniak on 25/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "object_model.h"
#include <stdlib.h>

namespace kf
{

void* object_new( vm_context* vm, object_type type, size_t size )
{
    // TEMP.
    void* p = calloc( 1, 8 + size );
    *(uint32_t*)p = size;
    object_header* header = (object_header*)( (char*)p + 4 );
    header->type = type;
    return header + 1;
}

size_t object_size( vm_context* vm, object* object )
{
    // TEMP.
    return *(uint32_t*)( (char*)object - 8 );
}

}

