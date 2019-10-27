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
#include "string_object.h"
#include <limits.h>
#include <functional>

namespace kf
{

/*
    Round up to next highest power of two.
*/

static inline size_t ceilpow2( size_t x )
{
    if ( x > 1 )
        return 1 << ( sizeof( size_t ) * CHAR_BIT - __builtin_clz( x - 1 ) );
    else
        return 1;
}

/*
    When used as keys, -0.0 becomes 0.0, and string pointers are marked.
*/

static inline value key_value( value value )
{
    if ( ! is_object( value ) || header( as_object( value ) )->type != STRING_OBJECT )
    {
        if ( value.v != number_value( -0.0 ).v )
            return value;
        else
            return number_value( +0.0 );
    }
    else
    {
        return { value.v | 4 };
    }
}

static inline bool is_string_key( value key )
{
    return ! is_number( key ) && ( key.v & 4 ) != 0;
}

static inline string_object* as_string_key( value key )
{
    return (string_object*)( key.v & ~(uint64_t)4 );
}

static inline size_t hash_key( value key )
{
    if ( ! is_string_key( key ) )
    {
        return std::hash< uint64_t >()( key.v );
    }
    else
    {
        string_object* string = as_string_key( key );
        return std::hash< std::string_view >()( std::string_view( string->text, string->size ) );
    }
}

static inline bool equal_key( value a, value b )
{
    if ( a.v == b.v )
    {
        return true;
    }

    if ( is_string_key( a ) && is_string_key( b ) )
    {
        string_object* sa = as_string_key( a );
        string_object* sb = as_string_key( b );
        if ( sa->size != sb->size )
        {
            return false;
        }
        return memcmp( sa->text, sb->text, sa->size ) == 0;
    }

    return false;
}

/*
    Key/value slot list.
*/

kvslots_object* kvslots_new( vm_context* vm, size_t count )
{
    kvslots_object* kvslots = (kvslots_object*)object_new( vm, KVSLOTS_OBJECT, sizeof( kvslots_object ) + ( count + 1 ) * sizeof( kvslot ) );
    kvslots->count = count;
    kvslots->slots[ count ].next = (kvslot*)-1;
    return kvslots;
}

/*
    Table functions.
*/

table_object* table_new( vm_context* vm, size_t capacity )
{
    table_object* table = (table_object*)object_new( vm, TABLE_OBJECT, sizeof( table_object ) );
    if ( capacity != 0 )
    {
        capacity += capacity / 4;
        capacity = ceilpow2( std::max< size_t >( capacity, 16 ) ) - 1;
        winit( table->kvslots, kvslots_new( vm, capacity ) );
    }
    return table;
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

