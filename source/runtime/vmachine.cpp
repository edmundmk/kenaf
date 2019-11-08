//
//  vmachine.cpp
//
//  Created by Edmund Kapusniak on 25/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "vmachine.h"
#include <stdlib.h>
#include "objects/lookup_object.h"

namespace kf
{

vmachine::vmachine()
    :   cothreads( nullptr )
    ,   global_object( nullptr )
    ,   prototypes{}
    ,   self_key( nullptr )
    ,   self_sel{}
    ,   next_cookie( 0 )
{
}

vmachine::~vmachine()
{
}

void* object_new( vmachine* vm, type_code type, size_t size )
{
    // TEMP.
    void* p = calloc( 1, 8 + size );
    *(uint32_t*)p = size;
    object_header* header = (object_header*)( (char*)p + 4 );
    header->type = type;
    return header + 1;
}

size_t object_size( vmachine* vm, object* object )
{
    // TEMP.
    return *(uint32_t*)( (char*)object - 8 );
}

void setup_object_model( vmachine* vm )
{
    // Prototype objects.
    lookup_object* object = lookup_new( vm, nullptr );
    vm->prototypes[ LOOKUP_OBJECT ] = object;
    vm->prototypes[ STRING_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ ARRAY_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ TABLE_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ FUNCTION_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ NATIVE_FUNCTION_OBJECT ] = vm->prototypes[ FUNCTION_OBJECT ];
    vm->prototypes[ COTHREAD_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ NUMBER_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ BOOL_OBJECT ] = lookup_new( vm, object );
    vm->prototypes[ NULL_OBJECT ] = lookup_new( vm, object );

    // 'self' key.
    vm->self_key = string_key( vm, "self", 4 );
}

lookup_object* value_keyerof( vmachine* vm, value v )
{
    if ( box_is_number( v ) )
    {
        return vm->prototypes[ NUMBER_OBJECT ];
    }
    else if ( box_is_string( v ) )
    {
        return vm->prototypes[ STRING_OBJECT ];
    }
    else if ( box_is_object( v ) )
    {
        type_code type = header( unbox_object( v ) )->type;
        if ( type == LOOKUP_OBJECT )
        {
            return (lookup_object*)unbox_object( v );
        }
        else
        {
            return vm->prototypes[ type ];
        }
    }
    else if ( box_is_bool( v ) )
    {
        return vm->prototypes[ BOOL_OBJECT ];
    }
    else
    {
        return vm->prototypes[ NULL_OBJECT ];
    }
}

lookup_object* value_superof( vmachine* vm, value v )
{
    if ( box_is_number( v ) )
    {
        return vm->prototypes[ NUMBER_OBJECT ];
    }
    else if ( box_is_string( v ) )
    {

        return vm->prototypes[ STRING_OBJECT ];
    }
    else if ( box_is_object( v ) )
    {
        type_code type = header( unbox_object( v ) )->type;
        if ( type == LOOKUP_OBJECT )
        {
            return lookup_prototype( vm, (lookup_object*)unbox_object( v ) );
        }
        else
        {
            return vm->prototypes[ type ];
        }
    }
    else if ( box_is_bool( v ) )
    {
        return vm->prototypes[ BOOL_OBJECT ];
    }
    else
    {
        return vm->prototypes[ NULL_OBJECT ];
    }
}

}

