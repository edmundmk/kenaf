//
//  table_object.h
//
//  Created by Edmund Kapusniak on 26/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_TABLE_OBJECT_H
#define KF_TABLE_OBJECT_H

/*
    Hash table mapping arbitrary values.
*/

#include "object_model.h"

namespace kf
{

/*
    Structures.
*/

struct keyval
{
    ref_value k;
    ref_value v;
    keyval* next;
};

struct kvslots_object : public object
{
    size_t count;
    keyval slots[];
};

struct table_object : public object
{
    ref< kvslots_object > kvslots;
    size_t length;
};

/*
    Functions.
*/

kvslots_object* kvslots_new( vm_context* vm, size_t count );
table_object* table_new( vm_context* vm, size_t capacity );
value table_getindex( vm_context* vm, table_object* table, value key );
void table_setindex( vm_context* vm, table_object* table, value key, value value );
void table_delindex( vm_context* vm, table_object* table, value key );
void table_clear( vm_context* vm, table_object* table );

}

#endif

