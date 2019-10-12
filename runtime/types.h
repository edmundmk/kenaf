//
//  types.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef TYPES_H
#define TYPES_H

/*
    Object types used by the runtime system.
*/

#include <vector>
#include <gc.h>

namespace kf
{

struct selector;
struct keyval;

struct o_layout;
struct o_object;
struct o_native_object;
struct o_string;
struct o_array;
struct o_table;
struct o_function;
struct o_native_function;
struct o_upval;
struct o_cothread;
struct o_code;
struct o_value_list;
struct o_keyval_list;

/*
    Type indexes.
*/

enum
{
    O_TYPE_NONE,
    O_TYPE_OBJECT,
    O_TYPE_NATIVE_OBJECT,
    O_TYPE_LAYOUT,
    O_TYPE_STRING,
    O_TYPE_ARRAY,
    O_TYPE_TABLE,
    O_TYPE_FUNCTION,
    O_TYPE_NATIVE_FUNCTION,
    O_TYPE_UPVAL,
    O_TYPE_COTHREAD,
    O_TYPE_CODE,
    O_TYPE_VALUE_LIST,
    O_TYPE_KEYVAL_LIST,
};

/*
    A selector caches the result of key lookup.
*/

struct selector
{
    uint32_t layout_cookie;
    uint32_t slot_index;
    gc_value* slot_pointer;
};

/*
    A single keyval entry in a table.
*/

struct keyval
{
    gc_value key;
    gc_value value;
    size_t next;
};

/*
    A layout represents a unique mapping from key strings to slot indexes.
    Layout records form a tree.  Each time a key is added to an object, the
    runtime looks up a layout with parent equal to the object's current layout
    and key equal to the key.  If it doesn't exist, it is created.
*/

struct o_layout
{
    gc_ref< o_object > prototype;
    gc_ref< o_layout > parent;
    gc_ref< o_string > key;
    uint32_t layout_cookie;
    uint32_t slot_index;
};

/*
    An object is a dynamic data structure which maps keys to values.  The
    mapping of keys to values is determined by the object's layout, which
    changes as keys are added.
*/

struct o_object
{
    gc_ref< o_layout > layout;
    gc_ref< o_value_list > slots;
};

/*
    A native object has a pointer to a native object.
*/

struct o_native_object : public o_object
{
    void* native;
};

/*
    A string.
*/

struct o_string
{
    uint32_t length;
    uint32_t hash;
    char c[];
};

/*
    An array is a dynamic data structure mapping integer indexes to values.
*/

struct o_array
{
    gc_ref< o_value_list > values;
    size_t length;
};

/*
    A table is a dynamic data structure mapping key values to values.
*/

struct o_table
{
    gc_ref< o_keyval_list > keyvals;
    size_t occupancy;
};

/*
    A function.
*/

struct o_function
{
    gc_ref< o_program > program;
    gc_value u[];
};

/*
    A native function.
*/

struct o_native_function
{
    void* native;
    void* cookie;
};

/*
    An upval.
*/

struct o_upval
{
    gc_ref< o_cothread > cothread;
    gc_value v;
};

/*
    A value list.
*/

struct o_value_list
{
    gc_value v[];
};

/*
    A keyval list.
*/

struct o_keyval_list
{
    o_keyval kv[]
};

}

#endif


/*
    Runtime has the following tables:

        roots       -> map[ value -> refcount ]                    // strong refs to values
        key_names   -> map[ string ]                             // weak ref to strings
        key_map     -> map[ layout, string -> index ]
        addkey_map  -> map[ layout, string -> layout ]
        proto_map   -> map[ object -> layout ]


    Entries in roots table are alive
        - while the refcount is non-zero.

    Entries in key_names table are alive as long as
        - the string is alive
        - any layout with this key is alive

    Layouts are alive while:
        - the layout is the root layout of a prototype and the object is alive.
        - any object using the layout is alive.
        - any layout that is descended from the layout is alive.

    Keys in the key map are alive while
        - the layout is alive

    Keys in the proto map are alive while
        - the object is alive


    key_names has a weak reference to strings.  so resurrection of keys
    requires careful thought.

    key_map has a weak reference to child layouts.  so resurrection of child
    layouts requires careful thought, too.

    maybe, once sweeping has begun, all unmarked objects are dead so you must
    act as if they are dead.


    We can mark roots like any other reference - if it is updated before the
    GC has got to it, then the mutator must ensure the marker gets the state
    at the start of the mark phase.


    Marking marks through the tables:

        - Objects mark their prototype in proto_map.
        - Layouts mark strings.
        -


    The only thing that can remove entries from the key_names, key_map, and
    proto_map tables is the sweeping process.

        - Once objects are swept, the entry in proto_map is removed.



    Objects are sealed once they are used as a prototype.

    Key lookups are cached in selectors, which look like:

        [ key / layout -> slot pointer or index ]

    Cothread stacks are marked eagerly.  This means that the entire state of
    the stack has to be marked before it can be modified.


    Concurrent GC.  On x86, mov gives us all the ordering guarantees we need.
    On ARM, section 6.3:
        http://infocenter.arm.com/help/topic/com.arm.doc.genc007826/Barrier_Litmus_Tests_and_Cookbook_A08.pdf

        create object and construct it
        dmb st
        store pointer to object (relaxed)

        read pointer to object (relaxed)
        access object through pointer

    Is fine.

*/
