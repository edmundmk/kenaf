//
//  table_object.cpp
//
//  Created by Edmund Kapusniak on 26/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "table_object.h"

namespace kf
{

kvslots_object* kvslots_new( vm_context* vm, size_t count )
{
    kvslots_object* kvslots = (kvslots_object*)object_new( vm, KVSLOTS_OBJECT, sizeof( kvslots_object ) + count * sizeof( kvslot ) );
    kvslots->count = count;
    return kvslots;
}

table_object* table_new( vm_context* vm, size_t capacity )
{

}

value table_getindex( vm_context* vm, table_object* table, value key )
{
}

void table_setindex( vm_context* vm, table_object* table, value key, value value )
{
}

void table_delindex( vm_context* vm, table_object* table, value key )
{
}

void table_clear( vm_context* vm, table_object* table )
{
}

}

