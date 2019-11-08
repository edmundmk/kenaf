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

#include "../vmachine.h"

namespace kf
{

/*
    Structures.
*/

struct kvslot
{
    ref_value k;
    ref_value v;
    kvslot* next;
};

struct kvslots_object : public object
{
    size_t count;
    kvslot slots[];
};

struct table_object : public object
{
    ref< kvslots_object > kvslots;
    size_t length;
};

/*
    Functions.
*/

kvslots_object* kvslots_new( vmachine* vm, size_t count );
table_object* table_new( vmachine* vm, size_t capacity );
value table_getindex( vmachine* vm, table_object* table, value key );
bool table_tryindex( vmachine* vm, table_object* table, value key, value* out_value );
void table_setindex( vmachine* vm, table_object* table, value key, value val );
void table_delindex( vmachine* vm, table_object* table, value key );
void table_clear( vmachine* vm, table_object* table );

/*
    Table for-each.
*/

struct table_keyval
{
    value k;
    value v;
};

size_t table_iterate( vmachine* vm, table_object* table );
bool table_next( vmachine* vm, table_object* table, size_t* i, table_keyval* keyval );

}

#endif

