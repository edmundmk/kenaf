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

static inline size_t key_hash( value key )
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

static inline bool key_equal( value a, value b )
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
    kvslots_object* kvslots = read( table->kvslots );
    if ( kvslots->count )
    {
        key = key_value( key );
        kvslot* slot = kvslots->slots + key_hash( key ) % kvslots->count;
        if ( slot->next ) do
        {
            if ( key_equal( read( slot->k ), key ) )
            {
                return read( slot->v );
            }
            slot = slot->next;
        }
        while ( slot != (kvslot*)-1 );
    }
    throw std::out_of_range( "table" );
}

static kvslot* table_insert( vm_context* vm, kvslots_object* kvslots, size_t kvcount, value key, kvslot* main_slot )
{
    // Insert in main slot if it's empty.
    if ( main_slot->next )
    {
        main_slot->next = (kvslot*)-1;
        return main_slot;
    }

    // Identify cuckoo.
    size_t cuckoo_hash = key_hash( read( main_slot->k ) );
    kvslot* cuckoo_main_slot = kvslots->slots + cuckoo_hash % kvcount;
    assert( cuckoo_main_slot->next );

    // Find nearby free slot.
    kvslot* free_slot = nullptr;
    for ( kvslot* slot = cuckoo_main_slot + 1; slot < kvslots->slots + kvcount; ++slot )
    {
        if ( ! slot->next )
        {
            free_slot = slot;
            break;
        }
    }

    if ( ! free_slot ) for ( kvslot* slot = cuckoo_main_slot - 1; slot >= kvslots->slots; --slot )
    {
        if ( ! slot->next )
        {
            free_slot = slot;
            break;
        }
    }

    // Check for hash collision if cuckoo and new entry hash to same bucket.
    if ( cuckoo_main_slot == main_slot )
    {
        free_slot->next = main_slot->next;
        main_slot->next = free_slot;
        return free_slot;
    }

    // Find previous slot in cuckoo's bucket's linked list.
    kvslot* prev_slot = cuckoo_main_slot;
    while ( prev_slot->next != main_slot )
    {
        prev_slot = prev_slot->next;
        assert( prev_slot != (kvslot*)-1 );
    }

    // Move item from main_slot to free_slot.
    write( vm, free_slot->k, read( main_slot->k ) );
    write( vm, free_slot->v, read( main_slot->v ) );

    // Update bucket list for amended cuckoo bucket.
    prev_slot->next = free_slot;
    free_slot->next = main_slot->next;
    main_slot->next = (kvslot*)-1;

    return main_slot;
}

void table_setindex( vm_context* vm, table_object* table, value key, value val )
{
    key = key_value( key );
    size_t hash = key_hash( key );
    kvslot* main_slot;

    kvslots_object* kvslots = read( table->kvslots );
    size_t kvcount = kvslots->count;

    if ( kvcount )
    {
        // Check if the key already exists in the table.
        kvslot* slot = main_slot = kvslots->slots + hash % kvcount;
        if ( slot->next ) do
        {
            if ( key_equal( read( slot->k ), key ) )
            {
                write( vm, slot->v, val );
                return;
            }
            slot = slot->next;
        }
        while ( slot != (kvslot*)-1 );
    }

    if ( table->length >= kvcount - ( kvcount / 8 ) )
    {
        // Reallocate kvslots with a larger count.
        size_t new_kvcount = std::max< size_t >( ( kvcount + 1 ) * 2, 16 ) - 1;
        kvslots_object* new_kvslots = kvslots_new( vm, new_kvcount );

        // Re-insert all elements.
        for ( size_t i = 0; i < kvcount; ++i )
        {
            kvslot* kval = kvslots->slots + i;
            if ( kval->next )
            {
                value key = read( kval->k );
                kvslot* slot = new_kvslots->slots + key_hash( key ) % new_kvcount;
                slot = table_insert( vm, new_kvslots, new_kvcount, key, slot );
                write( vm, slot->k, key );
                write( vm, slot->v, val );
            }
        }

        // Update.
        write( vm, table->kvslots, new_kvslots );
        kvslots = new_kvslots;
        kvcount = new_kvcount;

        // Recalculate main slot in reallocated slots list.
        main_slot = kvslots->slots + hash % kvcount;
    }

    // Insert.
    table->length += 1;
    kvslot* slot = table_insert( vm, kvslots, kvcount, key, main_slot );
    write( vm, slot->k, key );
    write( vm, slot->v, val );
}

void table_delindex( vm_context* vm, table_object* table, value key )
{
}

void table_clear( vm_context* vm, table_object* table )
{
}

}

