//
//  array_object.cpp
//
//  Created by Edmund Kapusniak on 26/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "array_object.h"

namespace kf
{

array_object* array_new( vm_context* vm, size_t capacity )
{
    return (array_object*)object_new( vm, ARRAY_OBJECT, sizeof( array_object ) );
}

void array_resize( vm_context* vm, array_object* array, size_t length )
{

}

void array_append( vm_context* vm, array_object* array, value value )
{
}

void array_extend( vm_context* vm, array_object* array, value* values, size_t vcount )
{
}

}

