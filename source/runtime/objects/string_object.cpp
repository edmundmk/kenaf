//
//  string_object.h
//
//  Created by Edmund Kapusniak on 25/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "string_object.h"

namespace kf
{

string_object* string_new( vm_context* vm, size_t size )
{
    return (string_object*)object_new( vm, sizeof( string_object ) + size );
}

string_object* string_key( vm_context* vm, string_object* string )
{

}

string_object* string_key( vm_context* vm, const char* text, size_t size )
{
}

}

