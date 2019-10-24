//
//  objects.h
//
//  Created by Edmund Kapusniak on 24/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef OBJECTS_H
#define OBJECTS_H

/*
    These are the data structures manipulated by the runtime.

      - Lookup object : Base class of all objects which store values of keys.
      - Native object : A lookup object with an associated native pointer.
      - Array object : An array indexed by an integer.
      - Table object : An associative array indexed by arbitrary values.
      - Value list : An array of values.
      - Keyval list : An array of keyval structures.
      - String object : A UTF-8 string.
      - Function object : An instance of a function.
      - Native function object : A wrapper for a native function.
      - Cothread object : The call and value stack for a coroutine.
      - Layout object : Describes the slot layout of a lookup object.
      - Code object : Stores a

*/

#include "gc.h"

namespace kf
{

struct keyval;
struct selector;

struct lookup_object;
struct native_object;
struct array_object;
struct table_object;
struct vlist_object;
struct kvlist_object;
struct string_object;
struct function_object;
struct native_function_object;
struct cothread_object;
struct layout_object;
struct code_object;

enum
{
    LOOKUP_OBJECT,
    NATIVE_OBJECT,
    LAYOUT_OBJECT,
    ARRAY_OBJECT,
    TABLE_OBJECT,
    VLIST_OBJECT,
    KVLIST_OBJECT,
    STRING_OBJECT,
    FUNCTION_OBJECT,
    NATIVE_FUNCTION_OBJECT,
    COTHREAD_OBJECT,
    CODE_OBJECT,
};

/*
    Lookup object.
*/

struct selector
{
    uint32_t cookie;
    uint32_t index;
    gc_value* p;
};

struct lookup_object
{
    gc_ref< layout_object > layout;
    gc_ref< vlist_object > slots;
};

struct native_object : public lookup_object
{
    void* native;
};

struct layout_object
{
    gc_ref< lookup_object > prototype;
    gc_ref< layout_object > parent;
    gc_ref< string_object > key;
    uint32_t cookie;
    uint32_t index;
};

value get_key( vm_context* context, lookup_object* lookup, selector* selector );
void set_key( vm_context* context, lookup_object* lookup, selector* selector, value v );

/*
    Array object.
*/

struct array_object
{
    gc_ref< vlist_object > values;
    size_t length;
};

value get_index( vm_context* context, array_object* array, size_t index );
void set_index( vm_context* context, array_object* array, size_t index, value v );

/*
    Table object.
*/

struct table_object
{
    gc_ref< kvlist_object > keyvals;
    size_t occupancy;
};

struct vlist_object
{
    gc_value v[];
};

struct klist_object
{
    size_t size;
    keyval kv[];
};

struct string_object
{
    uint32_t length;
    uint32_t hash;
    char s[];
};

struct function_object
{
    gc_ref< code_object > code;
    gc_ref< vlist_object > outenv[];
};

struct native_function_object
{
};

struct cothread_object
{
    std::vector< value > stack;
    std::vector< frame > frames;
};

struct code_object
{
};

}

#endif

